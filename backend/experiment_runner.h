#pragma once
#include "workload_generator.h"
#include "worker_pool.h"
#include "scheduler_fcfs.h"
#include "scheduler_adaptive.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    EXP_STATUS_IDLE = 0,
    EXP_STATUS_CALIBRATING = 1,
    EXP_STATUS_RUNNING_FCFS = 2,
    EXP_STATUS_FCFS_DONE = 3,
    EXP_STATUS_RUNNING_ADAPTIVE = 4,
    EXP_STATUS_COMPLETED = 5,
    EXP_STATUS_CANCELLED = 6,
    EXP_STATUS_ERROR = 7
} ExperimentStatus;

typedef struct {
    // Input settings
    DWORD_PTR affinity_mask;
    int affinity_core_count;
    double scale_factor;
    int custom_num_bg;
    int custom_num_interactive;
    double custom_bg_ms;
    double custom_interactive_ms;
    BOOL is_custom_workload;

    // Execution state
    ExperimentStatus status;
    char current_step_description[128];
    double progress_percent;
    double elapsed_seconds;

    // Machine context
    char cpu_model[128];
    int logical_cores;
    double host_cpu_at_start;
    unsigned long long cal_iters_per_ms;

    // Generated workload
    WorkloadBatch workload;

    // Results
    AlgorithmRunResult fcfs_result;
    AlgorithmRunResult adaptive_result;
    AdaptivePolicyContext adaptive_ctx;

    // Comparison Metrics
    double response_time_improvement_pct;
    double waiting_time_improvement_pct;
    double background_makespan_delta_pct;
    int preemption_count;
    int context_switch_delta;

    // Dynamic conclusion text generated strictly from real measurements
    char empirical_conclusion[1024];
    char trade_off_analysis[1024];
} ExperimentReport;

void experiment_init(void);
void experiment_prepare_workload(const char* mode, const char* intensity);
BOOL experiment_run_fcfs(DWORD_PTR affinity_mask);
BOOL experiment_inject_urgent_task(const char* name);
BOOL experiment_run_adaptive(DWORD_PTR affinity_mask);
BOOL experiment_start(DWORD_PTR affinity_mask, double scale_factor, BOOL use_contention_affinity);
void experiment_cancel(void);
void experiment_get_status(ExperimentReport* out);

#ifdef __cplusplus
}
#endif
