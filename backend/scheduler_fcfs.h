#pragma once
#include "worker_pool.h"

#ifdef __cplusplus
extern "C" {
#endif

// Runs the FCFS scheduling benchmark using real Win32 worker threads
// Returns TRUE on success, FALSE on cancellation or error
BOOL run_fcfs_benchmark(const WorkloadBatch* batch, DWORD_PTR affinity_mask, AlgorithmRunResult* out_result, volatile LONG* cancel_flag);

#ifdef __cplusplus
}
#endif
