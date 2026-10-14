#pragma once
#include "worker_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double host_cpu_load_percent;
    int aging_threshold_ms;     // Time in queue before priority increases by 1 (e.g. 150ms)
    double checkpoint_interval_ms; // Target preemption checkpoint granularity (e.g. 15ms)
    char adaptive_reasoning[256];
} AdaptivePolicyContext;

// Runs the Adaptive Priority Scheduler with Cooperative Preemption & Aging
BOOL run_adaptive_benchmark(const WorkloadBatch* batch, DWORD_PTR affinity_mask, AlgorithmRunResult* out_result, AdaptivePolicyContext* out_ctx, volatile LONG* cancel_flag);

#ifdef __cplusplus
}
#endif
