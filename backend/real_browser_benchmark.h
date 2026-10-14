#pragma once
#include <winsock2.h>
#include <windows.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_BENCHMARK_TABS 16
#define MAX_BG_WORKER_THREADS 8

typedef enum {
    REAL_BENCH_IDLE = 0,
    REAL_BENCH_RUNNING = 1,
    REAL_BENCH_COMPLETED = 2,
    REAL_BENCH_CANCELLED = 3
} RealBrowserBenchState;

typedef struct {
    int tab_id;
    double nav_load_ms;
    double dom_render_ms;
    double event_latency_ms;
    char url[256];
    double reported_at_ms;
} RealBrowserTabMetric;

typedef struct {
    char name[64];
    char exe_path[MAX_PATH];
    BOOL is_available;
} DetectedBrowser;

typedef struct {
    RealBrowserBenchState state;
    char current_policy[32]; // "fcfs" or "adaptive"
    char workload_type[32];  // "offline" or "online"
    int target_tab_count;
    int reported_tab_count;
    RealBrowserTabMetric tab_metrics[MAX_BENCHMARK_TABS];

    // Summary Tab Metrics
    double avg_nav_load_ms;
    double p95_nav_load_ms;
    double avg_event_latency_ms;

    // Background Computation Metrics
    BOOL bg_work_active;
    int bg_thread_count;
    int bg_current_priority; // e.g. THREAD_PRIORITY_NORMAL vs THREAD_PRIORITY_BELOW_NORMAL
    char bg_priority_label[64];
    double bg_work_duration_ms;
    unsigned long long bg_total_hashes_completed;

    // Execution timestamps & status
    double bench_elapsed_ms;
    char status_message[256];

    // Priority transitions history (for inspector)
    int transition_count;
    struct {
        double t_ms;
        int old_pri;
        int new_pri;
        char trigger[128];
    } transitions[32];
} RealBrowserRunResult;

typedef struct {
    RealBrowserRunResult fcfs_result;
    RealBrowserRunResult adaptive_result;
    BOOL has_fcfs;
    BOOL has_adaptive;
    double load_improvement_pct;
    double latency_improvement_pct;
    double bg_overhead_pct;
    char empirical_conclusion[512];
} RealBrowserComparisonReport;

void real_browser_bench_init(void);
void real_browser_detect_browsers(DetectedBrowser* out_chrome, DetectedBrowser* out_edge);
BOOL real_browser_start_benchmark(const char* policy, const char* workload_type, const char* browser_choice, int tab_count);
BOOL real_browser_record_tab_metric(int tab_id, const char* policy, double nav_load_ms, double dom_render_ms, double event_latency_ms, const char* url);
void real_browser_get_status(RealBrowserRunResult* out_current, RealBrowserComparisonReport* out_report);
void real_browser_cancel(void);

#ifdef __cplusplus
}
#endif
