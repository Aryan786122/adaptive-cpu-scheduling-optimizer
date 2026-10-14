#include "system_telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tlhelp32.h>
#include <psapi.h>

static CRITICAL_SECTION s_telem_cs;
static BOOL s_initialized = FALSE;

static FILETIME s_prev_idle;
static FILETIME s_prev_kernel;
static FILETIME s_prev_user;
static double s_last_cpu_percent = 0.0;
static ULONGLONG s_last_tick = 0;

static TelemetrySample s_history[TELEMETRY_HISTORY_LEN];
static int s_history_count = 0;

static ULONGLONG ft_to_ull(const FILETIME* ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft->dwLowDateTime;
    uli.HighPart = ft->dwHighDateTime;
    return uli.QuadPart;
}

static void query_cpu_model(char* out, size_t out_sz) {
    out[0] = '\0';
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD sz = (DWORD)out_sz;
        DWORD type = REG_SZ;
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, &type, (LPBYTE)out, &sz) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            char* p = out;
            while (*p == ' ') p++;
            if (p != out) memmove(out, p, strlen(p) + 1);
            size_t len = strlen(out);
            while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
            return;
        }
        RegCloseKey(hKey);
    }
    const char* env_cpu = getenv("PROCESSOR_IDENTIFIER");
    if (env_cpu && *env_cpu) {
        strncpy(out, env_cpu, out_sz - 1);
        out[out_sz - 1] = '\0';
    } else {
        strncpy(out, "x86_64 Compatible Processor", out_sz - 1);
    }
}

static void query_cores(int* physical, int* logical) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    *logical = (int)si.dwNumberOfProcessors;
    if (*logical <= 0) *logical = 1;

    DWORD len = 0;
    *physical = 0;
    GetLogicalProcessorInformation(NULL, &len);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buf = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(len);
        if (buf) {
            if (GetLogicalProcessorInformation(buf, &len)) {
                DWORD cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                for (DWORD i = 0; i < cnt; i++) {
                    if (buf[i].Relationship == RelationProcessorCore) {
                        (*physical)++;
                    }
                }
            }
            free(buf);
        }
    }
    if (*physical <= 0) {
        *physical = (*logical >= 2) ? (*logical / 2) : 1;
    }
}

static double compute_cpu_percent(void) {
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return s_last_cpu_percent;
    }

    ULONGLONG now = GetTickCount64();
    if (s_last_tick == 0) {
        s_prev_idle = idle;
        s_prev_kernel = kernel;
        s_prev_user = user;
        s_last_tick = now;
        return 0.0;
    }

    if (now - s_last_tick < 200) {
        return s_last_cpu_percent;
    }

    ULONGLONG i_now = ft_to_ull(&idle);
    ULONGLONG k_now = ft_to_ull(&kernel);
    ULONGLONG u_now = ft_to_ull(&user);

    ULONGLONG i_prev = ft_to_ull(&s_prev_idle);
    ULONGLONG k_prev = ft_to_ull(&s_prev_kernel);
    ULONGLONG u_prev = ft_to_ull(&s_prev_user);

    ULONGLONG i_diff = (i_now >= i_prev) ? (i_now - i_prev) : 0;
    ULONGLONG k_diff = (k_now >= k_prev) ? (k_now - k_prev) : 0;
    ULONGLONG u_diff = (u_now >= u_prev) ? (u_now - u_prev) : 0;

    ULONGLONG total = k_diff + u_diff;
    double pct = 0.0;
    if (total > 0 && total >= i_diff) {
        pct = (double)(total - i_diff) * 100.0 / (double)total;
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;
    }

    s_prev_idle = idle;
    s_prev_kernel = kernel;
    s_prev_user = user;
    s_last_tick = now;
    s_last_cpu_percent = pct;
    return pct;
}

static void query_foreground_app(char* title_out, size_t title_sz, char* exe_out, size_t exe_sz, DWORD* pid_out) {
    title_out[0] = '\0';
    exe_out[0] = '\0';
    *pid_out = 0;

    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        strncpy(title_out, "Desktop / None", title_sz - 1);
        strncpy(exe_out, "System", exe_sz - 1);
        return;
    }

    GetWindowTextA(hwnd, title_out, (int)title_sz - 1);
    title_out[title_sz - 1] = '\0';
    if (!title_out[0]) {
        strncpy(title_out, "Active Window", title_sz - 1);
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    *pid_out = pid;

    if (pid > 0) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            char path[MAX_PATH];
            DWORD path_len = sizeof(path);
            if (QueryFullProcessImageNameA(hProc, 0, path, &path_len)) {
                char* slash = strrchr(path, '\\');
                const char* src = slash ? (slash + 1) : path;
                size_t i = 0;
                while (i + 1 < exe_sz && src[i]) { exe_out[i] = src[i]; i++; }
                exe_out[i] = '\0';
            }
            CloseHandle(hProc);
        }
    }
    if (!exe_out[0]) {
        const char* def = "Unknown Application";
        size_t i = 0;
        while (i + 1 < exe_sz && def[i]) { exe_out[i] = def[i]; i++; }
        exe_out[i] = '\0';
    }
}

void telemetry_init(void) {
    if (!s_initialized) {
        InitializeCriticalSection(&s_telem_cs);
        GetSystemTimes(&s_prev_idle, &s_prev_kernel, &s_prev_user);
        s_last_tick = GetTickCount64();
        s_initialized = TRUE;
    }
}

double telemetry_get_instant_cpu(void) {
    telemetry_init();
    EnterCriticalSection(&s_telem_cs);
    double pct = compute_cpu_percent();
    LeaveCriticalSection(&s_telem_cs);
    return pct;
}

void telemetry_get(SystemTelemetry* out) {
    if (!out) return;
    telemetry_init();
    memset(out, 0, sizeof(SystemTelemetry));

    // Hostname
    DWORD hlen = sizeof(out->hostname);
    if (!GetComputerNameA(out->hostname, &hlen)) {
        strncpy(out->hostname, "Windows-PC", sizeof(out->hostname) - 1);
    }

    strncpy(out->os_name, "Windows 10/11", sizeof(out->os_name) - 1);
    strncpy(out->architecture, "x86_64", sizeof(out->architecture) - 1);

    query_cpu_model(out->cpu_model, sizeof(out->cpu_model));
    query_cores(&out->physical_cores, &out->logical_cores);

    // Contention affinity masks
    DWORD_PTR proc_mask = 0, sys_mask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask) && proc_mask > 0) {
        out->affinity_mask_all = proc_mask;
    } else {
        out->affinity_mask_all = ((DWORD_PTR)1 << (out->logical_cores > 31 ? 31 : out->logical_cores)) - 1;
    }
    out->affinity_mask_contention_single = 0x1; // Core 0
    out->affinity_mask_contention_dual = (out->logical_cores >= 2) ? 0x3 : 0x1; // Cores 0-1

    // Uptime
    ULONGLONG up_ms = GetTickCount64();
    out->uptime_seconds = (double)up_ms / 1000.0;
    long s = (long)out->uptime_seconds;
    long days = s / 86400;
    long hours = (s % 86400) / 3600;
    long mins = (s % 3600) / 60;
    long secs = s % 60;
    if (days > 0) snprintf(out->uptime_formatted, sizeof(out->uptime_formatted), "%ldd %ldh %ldm", days, hours, mins);
    else if (hours > 0) snprintf(out->uptime_formatted, sizeof(out->uptime_formatted), "%ldh %ldm %lds", hours, mins, secs);
    else snprintf(out->uptime_formatted, sizeof(out->uptime_formatted), "%ldm %lds", mins, secs);

    // RAM
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        out->total_ram_gb = (double)mem.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
        DWORDLONG used = mem.ullTotalPhys - mem.ullAvailPhys;
        out->used_ram_gb = (double)used / (1024.0 * 1024.0 * 1024.0);
        out->ram_percent = (double)mem.dwMemoryLoad;
    }

    // Processes and threads count
    out->total_processes = 0;
    out->total_threads = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                out->total_processes++;
                out->total_threads += pe.cntThreads;
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }

    // Foreground app
    query_foreground_app(out->foreground_app_title, sizeof(out->foreground_app_title),
                         out->foreground_app_exe, sizeof(out->foreground_app_exe),
                         &out->foreground_pid);

    // CPU % and history
    EnterCriticalSection(&s_telem_cs);
    out->cpu_percent = compute_cpu_percent();

    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char tbuf[16] = "00:00:00";
    if (tm_info) strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);

    if (s_history_count < TELEMETRY_HISTORY_LEN) {
        snprintf(s_history[s_history_count].timestamp, sizeof(s_history[s_history_count].timestamp), "%s", tbuf);
        s_history[s_history_count].cpu_percent = out->cpu_percent;
        s_history_count++;
    } else {
        memmove(&s_history[0], &s_history[1], sizeof(TelemetrySample) * (TELEMETRY_HISTORY_LEN - 1));
        snprintf(s_history[TELEMETRY_HISTORY_LEN - 1].timestamp, sizeof(s_history[TELEMETRY_HISTORY_LEN - 1].timestamp), "%s", tbuf);
        s_history[TELEMETRY_HISTORY_LEN - 1].cpu_percent = out->cpu_percent;
    }
    out->history_count = s_history_count;
    memcpy(out->history, s_history, sizeof(TelemetrySample) * s_history_count);
    LeaveCriticalSection(&s_telem_cs);
}

// ============================================================================
// SYSTEM ACTIVITY CAPTURE IMPLEMENTATION (STAGE 1 GUIDED LAB)
// ============================================================================

static SystemCaptureResult s_capture_res = {
    .is_capturing = FALSE,
    .is_complete = FALSE,
    .progress_percent = 0.0,
    .duration_seconds = 5,
    .elapsed_seconds = 0.0,
    .total_processes_observed = 0,
    .avg_cpu_percent = 0.0,
    .top_process_count = 0,
    .host_activity_summary = "Awaiting system activity capture. Click 'Capture System Activity' to sample active Windows processes.",
    .recommended_aging_threshold_ms = 150
};
static HANDLE s_capture_thread = NULL;

typedef struct {
    DWORD pid;
    char name[MAX_PATH];
    ULONGLONG start_cpu;
    ULONGLONG end_cpu;
    ULONGLONG delta_cpu;
    double cpu_pct;
} ProcSampleInternal;

static int compare_proc_deltas(const void* a, const void* b) {
    const ProcSampleInternal* pa = (const ProcSampleInternal*)a;
    const ProcSampleInternal* pb = (const ProcSampleInternal*)b;
    if (pb->delta_cpu > pa->delta_cpu) return 1;
    if (pb->delta_cpu < pa->delta_cpu) return -1;
    return 0;
}

static DWORD WINAPI capture_thread_proc(LPVOID lpParam) {
    int duration_sec = (int)(intptr_t)lpParam;
    if (duration_sec < 1) duration_sec = 5;
    if (duration_sec > 15) duration_sec = 15;

    EnterCriticalSection(&s_telem_cs);
    s_capture_res.is_capturing = TRUE;
    s_capture_res.is_complete = FALSE;
    s_capture_res.progress_percent = 0.0;
    s_capture_res.duration_seconds = duration_sec;
    s_capture_res.elapsed_seconds = 0.0;
    LeaveCriticalSection(&s_telem_cs);

    // Initial system times
    FILETIME sys_idle_start, sys_kernel_start, sys_user_start;
    GetSystemTimes(&sys_idle_start, &sys_kernel_start, &sys_user_start);

    // Allocate process tracking array
    int max_procs = 512;
    ProcSampleInternal* procs = (ProcSampleInternal*)calloc(max_procs, sizeof(ProcSampleInternal));
    int proc_count = 0;

    if (procs) {
        HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snap != INVALID_HANDLE_VALUE) {
            PROCESSENTRY32 pe;
            pe.dwSize = sizeof(pe);
            if (Process32First(snap, &pe)) {
                do {
                    if (proc_count >= max_procs) break;
                    if (pe.th32ProcessID == 0) continue; // Skip System Idle

                    HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pe.th32ProcessID);
                    if (hProc) {
                        FILETIME cr, ex, k, u;
                        if (GetProcessTimes(hProc, &cr, &ex, &k, &u)) {
                            procs[proc_count].pid = pe.th32ProcessID;
                            snprintf(procs[proc_count].name, sizeof(procs[proc_count].name), "%s", pe.szExeFile);
                            procs[proc_count].start_cpu = ft_to_ull(&k) + ft_to_ull(&u);
                            proc_count++;
                        }
                        CloseHandle(hProc);
                    }
                } while (Process32Next(snap, &pe));
            }
            CloseHandle(snap);
        }
    }

    // Sleep in 100ms intervals to track progress
    int total_intervals = duration_sec * 10;
    for (int i = 0; i < total_intervals; i++) {
        Sleep(100);
        EnterCriticalSection(&s_telem_cs);
        s_capture_res.elapsed_seconds = (double)(i + 1) * 0.1;
        s_capture_res.progress_percent = ((double)(i + 1) / (double)total_intervals) * 100.0;
        LeaveCriticalSection(&s_telem_cs);
    }

    // Final system times
    FILETIME sys_idle_end, sys_kernel_end, sys_user_end;
    GetSystemTimes(&sys_idle_end, &sys_kernel_end, &sys_user_end);

    ULONGLONG sys_k_diff = ft_to_ull(&sys_kernel_end) - ft_to_ull(&sys_kernel_start);
    ULONGLONG sys_u_diff = ft_to_ull(&sys_user_end) - ft_to_ull(&sys_user_start);
    ULONGLONG sys_i_diff = ft_to_ull(&sys_idle_end) - ft_to_ull(&sys_idle_start);
    ULONGLONG total_sys = sys_k_diff + sys_u_diff;

    double avg_cpu = 0.0;
    if (total_sys > 0 && total_sys >= sys_i_diff) {
        avg_cpu = (double)(total_sys - sys_i_diff) * 100.0 / (double)total_sys;
        if (avg_cpu < 0.0) avg_cpu = 0.0;
        if (avg_cpu > 100.0) avg_cpu = 100.0;
    }

    // Final process times & deltas
    if (procs && proc_count > 0) {
        for (int i = 0; i < proc_count; i++) {
            HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, procs[i].pid);
            if (hProc) {
                FILETIME cr, ex, k, u;
                if (GetProcessTimes(hProc, &cr, &ex, &k, &u)) {
                    procs[i].end_cpu = ft_to_ull(&k) + ft_to_ull(&u);
                    if (procs[i].end_cpu >= procs[i].start_cpu) {
                        procs[i].delta_cpu = procs[i].end_cpu - procs[i].start_cpu;
                    }
                }
                CloseHandle(hProc);
            }
            if (total_sys > 0 && procs[i].delta_cpu > 0) {
                procs[i].cpu_pct = (double)procs[i].delta_cpu * 100.0 / (double)total_sys;
                if (procs[i].cpu_pct > 100.0) procs[i].cpu_pct = 100.0;
            }
        }

        // Sort descending by delta_cpu
        qsort(procs, proc_count, sizeof(ProcSampleInternal), compare_proc_deltas);
    }

    // Save final capture results
    EnterCriticalSection(&s_telem_cs);
    s_capture_res.is_capturing = FALSE;
    s_capture_res.is_complete = TRUE;
    s_capture_res.progress_percent = 100.0;
    s_capture_res.elapsed_seconds = (double)duration_sec;
    s_capture_res.total_processes_observed = proc_count;
    s_capture_res.avg_cpu_percent = avg_cpu;

    // Pick top 5 processes
    int top_n = proc_count < 5 ? proc_count : 5;
    s_capture_res.top_process_count = top_n;
    for (int i = 0; i < top_n; i++) {
        s_capture_res.top_processes[i].pid = procs[i].pid;
        snprintf(s_capture_res.top_processes[i].name, sizeof(s_capture_res.top_processes[i].name), "%s", procs[i].name);
        s_capture_res.top_processes[i].cpu_percent = procs[i].cpu_pct;
    }

    // Isolate real browser processes (Chrome / Edge / Firefox)
    int b_count = 0;
    double b_cpu_total = 0.0;
    int top_b_count = 0;
    char detected_browser[32] = "None Detected";

    for (int i = 0; i < proc_count; i++) {
        const char* pname = procs[i].name;
        BOOL is_browser = (_stricmp(pname, "chrome.exe") == 0 ||
                           _stricmp(pname, "msedge.exe") == 0 ||
                           _stricmp(pname, "firefox.exe") == 0 ||
                           _stricmp(pname, "brave.exe") == 0);
        if (is_browser) {
            b_count++;
            b_cpu_total += procs[i].cpu_pct;
            if (top_b_count < 8) {
                s_capture_res.browser_processes[top_b_count].pid = procs[i].pid;
                snprintf(s_capture_res.browser_processes[top_b_count].name, sizeof(s_capture_res.browser_processes[top_b_count].name), "%s", pname);
                s_capture_res.browser_processes[top_b_count].cpu_percent = procs[i].cpu_pct;
                top_b_count++;
            }
            if (_stricmp(pname, "chrome.exe") == 0) {
                snprintf(detected_browser, sizeof(detected_browser), "Google Chrome");
            } else if (_stricmp(pname, "msedge.exe") == 0 && strcmp(detected_browser, "Google Chrome") != 0) {
                snprintf(detected_browser, sizeof(detected_browser), "Microsoft Edge");
            }
        }
    }
    s_capture_res.browser_process_count = b_count;
    s_capture_res.browser_total_cpu = b_cpu_total;
    s_capture_res.top_browser_count = top_b_count;
    snprintf(s_capture_res.detected_browser_name, sizeof(s_capture_res.detected_browser_name), "%s", detected_browser);

    // Adaptive policy parameter derivation
    if (avg_cpu > 35.0) {
        s_capture_res.recommended_aging_threshold_ms = 120;
        snprintf(s_capture_res.host_activity_summary, sizeof(s_capture_res.host_activity_summary),
            "System activity captured (%.1fs sample): Host load is elevated (%.1f%%) with %d observed processes. Top CPU consumer: %s (%.1f%%). Dynamic aging threshold accelerated to 120ms to prevent starvation under host contention.",
            (double)duration_sec, avg_cpu, proc_count, top_n > 0 ? s_capture_res.top_processes[0].name : "system", top_n > 0 ? s_capture_res.top_processes[0].cpu_percent : 0.0);
    } else {
        s_capture_res.recommended_aging_threshold_ms = 150;
        snprintf(s_capture_res.host_activity_summary, sizeof(s_capture_res.host_activity_summary),
            "System activity captured (%.1fs sample): Host load is nominal (%.1f%%) with %d observed processes. Top active: %s (%.1f%%). Adaptive aging threshold set to 150ms for optimal interactive responsiveness.",
            (double)duration_sec, avg_cpu, proc_count, top_n > 0 ? s_capture_res.top_processes[0].name : "system", top_n > 0 ? s_capture_res.top_processes[0].cpu_percent : 0.0);
    }
    LeaveCriticalSection(&s_telem_cs);

    if (procs) free(procs);
    return 0;
}

BOOL telemetry_start_capture(int duration_seconds) {
    telemetry_init();
    EnterCriticalSection(&s_telem_cs);
    if (s_capture_res.is_capturing) {
        LeaveCriticalSection(&s_telem_cs);
        return FALSE; // Capture already in progress
    }
    LeaveCriticalSection(&s_telem_cs);

    if (s_capture_thread) {
        CloseHandle(s_capture_thread);
        s_capture_thread = NULL;
    }

    s_capture_thread = CreateThread(NULL, 0, capture_thread_proc, (LPVOID)(intptr_t)duration_seconds, 0, NULL);
    return (s_capture_thread != NULL);
}

void telemetry_get_capture_status(SystemCaptureResult* out) {
    if (!out) return;
    telemetry_init();
    EnterCriticalSection(&s_telem_cs);
    memcpy(out, &s_capture_res, sizeof(SystemCaptureResult));
    LeaveCriticalSection(&s_telem_cs);
}

