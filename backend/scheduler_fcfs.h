#pragma once
#include "worker_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    volatile LONG requested;
    char name[64];
} UrgentInjectionSlot;

// Runs the FCFS scheduling benchmark using real Win32 worker threads
// Supports dynamic task injection while running
// Returns TRUE on success, FALSE on cancellation or error
BOOL run_fcfs_benchmark(WorkloadBatch* batch, DWORD_PTR affinity_mask, AlgorithmRunResult* out_result, UrgentInjectionSlot* injection_slot, volatile LONG* cancel_flag);

#ifdef __cplusplus
}
#endif
