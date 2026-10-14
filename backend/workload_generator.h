#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MAX_WORKLOAD_TASKS 16

typedef enum {
    TASK_TYPE_BACKGROUND_CPU,
    TASK_TYPE_BACKGROUND_MEM,
    TASK_TYPE_INTERACTIVE
} ManagedTaskType;

typedef struct {
    int id;
    char name[64];
    ManagedTaskType type;
    int base_priority;          // Lower = higher priority (e.g. 1 for interactive, 5 for background)
    double target_duration_ms;  // Nominal calibrated work duration in ms
    unsigned long long total_iterations; // Calibrated computational iterations
    double arrival_offset_ms;   // Arrival timestamp relative to experiment t=0
    char description[128];
} ManagedTaskDef;

typedef struct {
    char profile_name[64];
    int task_count;
    ManagedTaskDef tasks[MAX_WORKLOAD_TASKS];
    double total_work_ms;
    int background_count;
    int interactive_count;
} WorkloadBatch;

// Generates the standard repeatable test workload
// Long background jobs start at t=0; short interactive tasks arrive during their execution
void workload_generate_standard(WorkloadBatch* out, double scale_factor);

// Generates custom parameterized workload
void workload_generate_custom(WorkloadBatch* out, int num_bg, int num_interactive, double bg_duration_ms, double interactive_duration_ms);

// Generates the 3-task guided laboratory workload (B1, B2, I1)
void workload_prepare_lab(WorkloadBatch* out, const char* mode, const char* intensity);

// Dynamically injects an urgent interactive task into an existing batch
int workload_inject_interactive_task(WorkloadBatch* batch, double arrival_offset_ms, const char* name);

#ifdef __cplusplus
}
#endif
