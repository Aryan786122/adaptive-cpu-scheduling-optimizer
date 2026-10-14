#include "real_browser_benchmark.h"
#include "http_server.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

static CRITICAL_SECTION s_bench_cs;
static BOOL s_initialized = FALSE;

static RealBrowserRunResult s_current_run;
static RealBrowserComparisonReport s_report;

static HANDLE s_bg_threads[MAX_BG_WORKER_THREADS];
static int s_bg_thread_count = 0;
static volatile LONG s_bg_running = 0;
static volatile LONG s_cancel_flag = 0;
static HANDLE s_browser_process = NULL;
static DWORD s_browser_pid = 0;

static LARGE_INTEGER s_qpc_freq;
static LARGE_INTEGER s_bench_start_qpc;

static double qpc_diff_ms(LARGE_INTEGER start, LARGE_INTEGER end) {
    if (s_qpc_freq.QuadPart == 0) return 0.0;
    return ((double)(end.QuadPart - start.QuadPart) * 1000.0) / (double)s_qpc_freq.QuadPart;
}

static void log_transition(RealBrowserRunResult* res, double t_ms, int old_pri, int new_pri, const char* trigger) {
    if (res->transition_count < 32) {
        int idx = res->transition_count++;
        res->transitions[idx].t_ms = t_ms;
        res->transitions[idx].old_pri = old_pri;
        res->transitions[idx].new_pri = new_pri;
        snprintf(res->transitions[idx].trigger, sizeof(res->transitions[idx].trigger), "%s", trigger);
    }
}

// Background Worker Thread: Computes cryptographic hashes and matrix operations
static DWORD WINAPI bg_worker_proc(LPVOID lpParam) {
    (void)lpParam;
    const size_t BUF_SIZE = 1024 * 1024; // 1 MB buffer per thread
    unsigned char* buf = (unsigned char*)malloc(BUF_SIZE);
    if (!buf) return 0;

    for (size_t i = 0; i < BUF_SIZE; i++) {
        buf[i] = (unsigned char)(i & 0xFF);
    }

    while (InterlockedCompareExchange(&s_bg_running, 0, 0) == 1) {
        // Compute pseudo-SHA256 chunk rounds
        unsigned int h = 0x6a09e667;
        for (size_t i = 0; i < BUF_SIZE; i += 64) {
            for (int r = 0; r < 64; r++) {
                h = ((h << 5) | (h >> 27)) + buf[i + r] + 0x428a2f98;
                h ^= (h >> 11);
            }
        }
        buf[0] = (unsigned char)(h & 0xFF);

        EnterCriticalSection(&s_bench_cs);
        s_current_run.bg_total_hashes_completed += 1000;
        LeaveCriticalSection(&s_bench_cs);

        // Cooperative yield slice
        Sleep(1);
    }

    free(buf);
    return 0;
}

void real_browser_bench_init(void) {
    if (s_initialized) return;
    InitializeCriticalSection(&s_bench_cs);
    QueryPerformanceFrequency(&s_qpc_freq);
    memset(&s_current_run, 0, sizeof(s_current_run));
    memset(&s_report, 0, sizeof(s_report));
    s_initialized = TRUE;
}

void real_browser_detect_browsers(DetectedBrowser* out_chrome, DetectedBrowser* out_edge) {
    char local_app_data[MAX_PATH] = "";
    GetEnvironmentVariableA("LOCALAPPDATA", local_app_data, sizeof(local_app_data));

    if (out_chrome) {
        memset(out_chrome, 0, sizeof(DetectedBrowser));
        strncpy(out_chrome->name, "Google Chrome", sizeof(out_chrome->name) - 1);
        char user_chrome[MAX_PATH];
        snprintf(user_chrome, sizeof(user_chrome), "%s\\Google\\Chrome\\Application\\chrome.exe", local_app_data);

        const char* chrome_paths[] = {
            "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe",
            "C:\\Program Files (x86)\\Google\\Chrome\\Application\\chrome.exe",
            user_chrome
        };
        for (int i = 0; i < 3; i++) {
            DWORD attr = GetFileAttributesA(chrome_paths[i]);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                strncpy(out_chrome->exe_path, chrome_paths[i], sizeof(out_chrome->exe_path) - 1);
                out_chrome->is_available = TRUE;
                break;
            }
        }
    }

    if (out_edge) {
        memset(out_edge, 0, sizeof(DetectedBrowser));
        strncpy(out_edge->name, "Microsoft Edge", sizeof(out_edge->name) - 1);
        char user_edge[MAX_PATH];
        snprintf(user_edge, sizeof(user_edge), "%s\\Microsoft\\Edge\\Application\\msedge.exe", local_app_data);

        const char* edge_paths[] = {
            "C:\\Program Files (x86)\\Microsoft\\Edge\\Application\\msedge.exe",
            "C:\\Program Files\\Microsoft\\Edge\\Application\\msedge.exe",
            user_edge
        };
        for (int i = 0; i < 3; i++) {
            DWORD attr = GetFileAttributesA(edge_paths[i]);
            if (attr != INVALID_FILE_ATTRIBUTES && !(attr & FILE_ATTRIBUTE_DIRECTORY)) {
                strncpy(out_edge->exe_path, edge_paths[i], sizeof(out_edge->exe_path) - 1);
                out_edge->is_available = TRUE;
                break;
            }
        }
    }
}

// Background scheduler thread: watches browser activity and modulates background threads
static DWORD WINAPI scheduler_watchdog_proc(LPVOID lpParam) {
    (void)lpParam;
    EnterCriticalSection(&s_bench_cs);
    BOOL is_adaptive = (strcmp(s_current_run.current_policy, "adaptive") == 0);
    LeaveCriticalSection(&s_bench_cs);

    LARGE_INTEGER t_now;
    double last_aging_boost_ms = 0.0;
    BOOL currently_aged_boosted = FALSE;

    while (InterlockedCompareExchange(&s_bg_running, 0, 0) == 1) {
        QueryPerformanceCounter(&t_now);
        double elapsed = qpc_diff_ms(s_bench_start_qpc, t_now);

        EnterCriticalSection(&s_bench_cs);
        s_current_run.bench_elapsed_ms = elapsed;
        int reported = s_current_run.reported_tab_count;
        int target = s_current_run.target_tab_count;
        LeaveCriticalSection(&s_bench_cs);

        // Check if all tabs have reported
        if (reported >= target && target > 0) {
            break;
        }

        // Timeout fallback after 16 seconds
        if (elapsed > 16000.0) {
            break;
        }

        if (is_adaptive) {
            // Under Adaptive Scheduling:
            // When browser tabs are still loading (reported < target):
            // Check if aging boost is needed (prevent background starvation)
            if (elapsed - last_aging_boost_ms > 1200.0 && !currently_aged_boosted) {
                // Boost to normal priority for a 150ms quantum to prevent starvation
                for (int i = 0; i < s_bg_thread_count; i++) {
                    SetThreadPriority(s_bg_threads[i], THREAD_PRIORITY_NORMAL);
                }
                EnterCriticalSection(&s_bench_cs);
                s_current_run.bg_current_priority = THREAD_PRIORITY_NORMAL;
                strncpy(s_current_run.bg_priority_label, "NORMAL (Aged Boost to prevent starvation)", sizeof(s_current_run.bg_priority_label) - 1);
                log_transition(&s_current_run, elapsed, THREAD_PRIORITY_LOWEST, THREAD_PRIORITY_NORMAL,
                               "Aging threshold (1200ms) reached: temporary boost to ensure background progress");
                LeaveCriticalSection(&s_bench_cs);
                currently_aged_boosted = TRUE;
                last_aging_boost_ms = elapsed;
            } else if (currently_aged_boosted && (elapsed - last_aging_boost_ms > 180.0)) {
                // Re-demote to lowest priority while browser tabs are loading
                for (int i = 0; i < s_bg_thread_count; i++) {
                    SetThreadPriority(s_bg_threads[i], THREAD_PRIORITY_LOWEST);
                }
                EnterCriticalSection(&s_bench_cs);
                s_current_run.bg_current_priority = THREAD_PRIORITY_LOWEST;
                strncpy(s_current_run.bg_priority_label, "LOWEST (Demoted to protect browser responsiveness)", sizeof(s_current_run.bg_priority_label) - 1);
                log_transition(&s_current_run, elapsed, THREAD_PRIORITY_NORMAL, THREAD_PRIORITY_LOWEST,
                               "Quantum expired: returned to background priority while browser is active");
                LeaveCriticalSection(&s_bench_cs);
                currently_aged_boosted = FALSE;
            }
        }

        Sleep(50);
    }

    // Stop background work
    InterlockedExchange(&s_bg_running, 0);

    QueryPerformanceCounter(&t_now);
    double total_ms = qpc_diff_ms(s_bench_start_qpc, t_now);

    EnterCriticalSection(&s_bench_cs);
    s_current_run.bg_work_active = FALSE;
    s_current_run.bg_work_duration_ms = total_ms;
    s_current_run.bench_elapsed_ms = total_ms;
    s_current_run.state = REAL_BENCH_COMPLETED;
    snprintf(s_current_run.status_message, sizeof(s_current_run.status_message),
             "%s run completed: %d/%d tabs reported. Average page load: %.1f ms.",
             is_adaptive ? "Adaptive Priority" : "FCFS Baseline",
             s_current_run.reported_tab_count, s_current_run.target_tab_count,
             s_current_run.avg_nav_load_ms);

    // Save to report
    if (is_adaptive) {
        s_report.adaptive_result = s_current_run;
        s_report.has_adaptive = TRUE;
    } else {
        s_report.fcfs_result = s_current_run;
        s_report.has_fcfs = TRUE;
    }

    // If both completed, compute comparative performance
    if (s_report.has_fcfs && s_report.has_adaptive) {
        double f_load = s_report.fcfs_result.avg_nav_load_ms;
        double a_load = s_report.adaptive_result.avg_nav_load_ms;
        if (f_load > 0.1) {
            s_report.load_improvement_pct = ((f_load - a_load) / f_load) * 100.0;
        }

        double f_lat = s_report.fcfs_result.avg_event_latency_ms;
        double a_lat = s_report.adaptive_result.avg_event_latency_ms;
        if (f_lat > 0.1) {
            s_report.latency_improvement_pct = ((f_lat - a_lat) / f_lat) * 100.0;
        }

        double f_bg = s_report.fcfs_result.bg_work_duration_ms;
        double a_bg = s_report.adaptive_result.bg_work_duration_ms;
        if (f_bg > 0.1) {
            s_report.bg_overhead_pct = ((a_bg - f_bg) / f_bg) * 100.0;
        }

        snprintf(s_report.empirical_conclusion, sizeof(s_report.empirical_conclusion),
                 "Adaptive priority modulation reduced real browser tab load latency by %.1f%% (from %.1fms to %.1fms) and event loop delay by %.1f%% under heavy background hashing, with a background execution makespan change of %+.1f%%.",
                 s_report.load_improvement_pct, f_load, a_load,
                 s_report.latency_improvement_pct,
                 s_report.bg_overhead_pct);
    }
    LeaveCriticalSection(&s_bench_cs);

    return 0;
}

BOOL real_browser_start_benchmark(const char* policy, const char* workload_type, const char* browser_choice, int tab_count) {
    real_browser_bench_init();

    EnterCriticalSection(&s_bench_cs);
    if (s_current_run.state == REAL_BENCH_RUNNING) {
        LeaveCriticalSection(&s_bench_cs);
        return FALSE;
    }

    if (tab_count < 1) tab_count = 10;
    if (tab_count > MAX_BENCHMARK_TABS) tab_count = MAX_BENCHMARK_TABS;

    memset(&s_current_run, 0, sizeof(s_current_run));
    s_current_run.state = REAL_BENCH_RUNNING;
    strncpy(s_current_run.current_policy, policy ? policy : "fcfs", sizeof(s_current_run.current_policy) - 1);
    strncpy(s_current_run.workload_type, workload_type ? workload_type : "offline", sizeof(s_current_run.workload_type) - 1);
    s_current_run.target_tab_count = tab_count;
    s_current_run.reported_tab_count = 0;
    s_current_run.bg_work_active = TRUE;
    s_current_run.bg_total_hashes_completed = 0;

    BOOL is_adaptive = (strcmp(s_current_run.current_policy, "adaptive") == 0);
    s_current_run.bg_current_priority = is_adaptive ? THREAD_PRIORITY_LOWEST : THREAD_PRIORITY_NORMAL;
    strncpy(s_current_run.bg_priority_label,
            is_adaptive ? "LOWEST (Demoted to protect browser responsiveness)" : "NORMAL (Baseline priority)",
            sizeof(s_current_run.bg_priority_label) - 1);

    snprintf(s_current_run.status_message, sizeof(s_current_run.status_message),
             "Launching %s benchmark with %d real browser tabs and background hashing...",
             is_adaptive ? "Adaptive Priority" : "FCFS Baseline", tab_count);

    QueryPerformanceCounter(&s_bench_start_qpc);
    log_transition(&s_current_run, 0.0, THREAD_PRIORITY_NORMAL, s_current_run.bg_current_priority,
                   is_adaptive ? "Initial dispatch: demoted background threads to protect browser time slices" : "Initial dispatch: background threads running at NORMAL priority");

    LeaveCriticalSection(&s_bench_cs);

    // 1. Launch background hashing threads (6-8 threads to generate real CPU contention)
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    int threads_to_spawn = si.dwNumberOfProcessors > 8 ? 8 : (int)si.dwNumberOfProcessors;
    if (threads_to_spawn < 2) threads_to_spawn = 2;
    s_bg_thread_count = threads_to_spawn;

    EnterCriticalSection(&s_bench_cs);
    s_current_run.bg_thread_count = threads_to_spawn;
    LeaveCriticalSection(&s_bench_cs);

    InterlockedExchange(&s_bg_running, 1);
    for (int i = 0; i < threads_to_spawn; i++) {
        s_bg_threads[i] = CreateThread(NULL, 0, bg_worker_proc, NULL, 0, NULL);
        if (s_bg_threads[i]) {
            SetThreadPriority(s_bg_threads[i], s_current_run.bg_current_priority);
        }
    }

    // 2. Detect and launch real browser instance
    DetectedBrowser chrome, edge;
    real_browser_detect_browsers(&chrome, &edge);
    const char* exe_path = NULL;

    if (browser_choice && _stricmp(browser_choice, "edge") == 0 && edge.is_available) {
        exe_path = edge.exe_path;
    } else if (chrome.is_available) {
        exe_path = chrome.exe_path;
    } else if (edge.is_available) {
        exe_path = edge.exe_path;
    }

    if (exe_path) {
        int port = http_server_get_port();
        if (port <= 0) port = 8000;

        char cmd_line[4096];
        char temp_dir[MAX_PATH];
        GetTempPathA(sizeof(temp_dir), temp_dir);
        char profile_dir[MAX_PATH + 64];
        snprintf(profile_dir, sizeof(profile_dir), "%ssched_browser_test_%s_%u", temp_dir, policy, (unsigned int)GetTickCount());

        int offset = snprintf(cmd_line, sizeof(cmd_line),
                              "\"%s\" --new-window --user-data-dir=\"%s\" --no-first-run --no-default-browser-check --disable-background-networking --disable-background-timer-throttling --disable-backgrounding-occluded-windows --disable-renderer-backgrounding",
                              exe_path, profile_dir);

        // Append 10 tab URLs
        for (int i = 1; i <= tab_count; i++) {
            if (offset < (int)sizeof(cmd_line) - 150) {
                offset += snprintf(cmd_line + offset, sizeof(cmd_line) - offset,
                                   " \"http://127.0.0.1:%d/benchmark_tab.html?tab=%d&policy=%s&workload=%s\"",
                                   port, i, policy, workload_type ? workload_type : "offline");
            }
        }

        STARTUPINFOA sa;
        memset(&sa, 0, sizeof(sa));
        sa.cb = sizeof(sa);
        PROCESS_INFORMATION pi;
        memset(&pi, 0, sizeof(pi));

        if (CreateProcessA(NULL, cmd_line, NULL, NULL, FALSE, 0, NULL, NULL, &sa, &pi)) {
            s_browser_process = pi.hProcess;
            s_browser_pid = pi.dwProcessId;
            CloseHandle(pi.hThread);
        }
    }

    // 3. Launch watchdog thread to monitor experiment completion and apply aging
    CreateThread(NULL, 0, scheduler_watchdog_proc, NULL, 0, NULL);
    return TRUE;
}

BOOL real_browser_record_tab_metric(int tab_id, const char* policy, double nav_load_ms, double dom_render_ms, double event_latency_ms, const char* url) {
    (void)policy;
    EnterCriticalSection(&s_bench_cs);

    if (s_current_run.reported_tab_count < MAX_BENCHMARK_TABS) {
        int idx = s_current_run.reported_tab_count++;
        s_current_run.tab_metrics[idx].tab_id = tab_id;
        s_current_run.tab_metrics[idx].nav_load_ms = nav_load_ms;
        s_current_run.tab_metrics[idx].dom_render_ms = dom_render_ms;
        s_current_run.tab_metrics[idx].event_latency_ms = event_latency_ms;
        if (url) strncpy(s_current_run.tab_metrics[idx].url, url, sizeof(s_current_run.tab_metrics[idx].url) - 1);

        LARGE_INTEGER t_now;
        QueryPerformanceCounter(&t_now);
        s_current_run.tab_metrics[idx].reported_at_ms = qpc_diff_ms(s_bench_start_qpc, t_now);

        // Recalculate averages
        double total_load = 0.0;
        double total_event = 0.0;
        for (int i = 0; i < s_current_run.reported_tab_count; i++) {
            total_load += s_current_run.tab_metrics[i].nav_load_ms;
            total_event += s_current_run.tab_metrics[i].event_latency_ms;
        }
        s_current_run.avg_nav_load_ms = total_load / (double)s_current_run.reported_tab_count;
        s_current_run.avg_event_latency_ms = total_event / (double)s_current_run.reported_tab_count;
    }

    LeaveCriticalSection(&s_bench_cs);
    return TRUE;
}

void real_browser_get_status(RealBrowserRunResult* out_current, RealBrowserComparisonReport* out_report) {
    EnterCriticalSection(&s_bench_cs);
    if (out_current) {
        *out_current = s_current_run;
    }
    if (out_report) {
        *out_report = s_report;
    }
    LeaveCriticalSection(&s_bench_cs);
}

void real_browser_cancel(void) {
    InterlockedExchange(&s_bg_running, 0);
    EnterCriticalSection(&s_bench_cs);
    s_current_run.state = REAL_BENCH_CANCELLED;
    s_current_run.bg_work_active = FALSE;
    strncpy(s_current_run.status_message, "Real browser benchmark cancelled by user.", sizeof(s_current_run.status_message) - 1);
    if (s_browser_process) {
        TerminateProcess(s_browser_process, 0);
        CloseHandle(s_browser_process);
        s_browser_process = NULL;
    }
    LeaveCriticalSection(&s_bench_cs);
}
