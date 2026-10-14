#pragma once
#include <windows.h>
#include "workload_generator.h"

#ifdef __cplusplus
extern "C" {
#endif

static inline void safe_copy_str(char* dst, const char* src, size_t max_len) {
    if (!dst || max_len == 0) return;
    if (!src) { dst[0] = '\0'; return; }
    size_t i = 0;
    while (i + 1 < max_len && src[i] != '\0') {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

#define MAX_TIMELINE_EVENTS 256
#define MAX_TRANSITIONS_LOG 512

typedef enum {
    STATE_NEW = 0,
    STATE_READY = 1,
    STATE_RUNNING = 2,
    STATE_YIELDED = 3,
    STATE_COMPLETED = 4
} TaskExecutionState;

typedef struct {
    double timestamp_ms;
    int task_id;
    char task_name[64];
    TaskExecutionState from_state;
    TaskExecutionState to_state;
    char trigger_reason[64]; // "Arrival", "Dispatched", "Preempted / Yield", "Completed"
} TaskTransition;

typedef struct {
    int task_id;
    char task_name[64];
    ManagedTaskType type;
    double start_ms;
    double end_ms;
    double duration_ms;
    int was_preempted;
} ExecutionSlice;

typedef struct {
    ManagedTaskDef def;
    TaskExecutionState current_state;
    int effective_priority; // Base priority minus aging bonus
    unsigned long long remaining_iterations;
    unsigned long long completed_iterations;
    
    // Exact measured metrics
    double actual_arrival_ms;
    double first_dispatch_ms;
    double completion_ms;
    double total_cpu_time_ms;
    double waiting_time_ms;
    double turnaround_time_ms;
    double response_time_ms;
    int times_preempted;
    int times_dispatched;

    // Worker thread runtime context
    HANDLE h_thread;
    HANDLE h_run_event;
    HANDLE h_yielded_event;
    HANDLE h_completed_event;
    volatile LONG yield_flag;
    volatile LONG terminate_flag;
    DWORD_PTR target_affinity;
} ManagedWorker;

typedef struct {
    char algorithm_name[64];
    char algorithm_code[16]; // "FCFS" or "ADAPTIVE"
    int task_count;
    ManagedWorker workers[MAX_WORKLOAD_TASKS];
    
    // Aggregated metrics
    double avg_interactive_response_time_ms;
    double p95_interactive_response_time_ms;
    double avg_interactive_waiting_time_ms;
    double avg_background_turnaround_time_ms;
    double background_makespan_ms;
    double total_benchmark_duration_ms;
    int total_preemptions;
    int total_context_switches;
    
    // Fine-grained timeline
    int slice_count;
    ExecutionSlice slices[MAX_TIMELINE_EVENTS];
    int transition_count;
    TaskTransition transitions[MAX_TRANSITIONS_LOG];
} AlgorithmRunResult;

#ifdef __cplusplus
}
#endif
