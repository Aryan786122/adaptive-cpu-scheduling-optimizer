#include "workload_generator.h"
#include "calibration.h"
#include <stdio.h>
#include <string.h>

void workload_generate_standard(WorkloadBatch* out, double scale_factor) {
    if (!out) return;
    memset(out, 0, sizeof(WorkloadBatch));
    strncpy(out->profile_name, "Standard Convoy Contention Benchmark", sizeof(out->profile_name) - 1);

    if (scale_factor < 0.2) scale_factor = 1.0;
    const MachineCalibration* cal = calibration_get();
    unsigned long long iters_per_ms = cal->iterations_per_ms;

    // Define 6 realistic tasks with scheduled arrival offsets
    // Task 1: Long CPU Background job starting at t=0
    out->tasks[0].id = 1;
    strncpy(out->tasks[0].name, "BG_Matrix_Compute_1", sizeof(out->tasks[0].name) - 1);
    out->tasks[0].type = TASK_TYPE_BACKGROUND_CPU;
    out->tasks[0].base_priority = 5;
    out->tasks[0].target_duration_ms = 600.0 * scale_factor;
    out->tasks[0].total_iterations = (unsigned long long)(out->tasks[0].target_duration_ms * (double)iters_per_ms);
    out->tasks[0].arrival_offset_ms = 0.0;
    strncpy(out->tasks[0].description, "CPU-intensive matrix multiplication batch", sizeof(out->tasks[0].description) - 1);

    // Task 2: Short Interactive job arriving at t=150ms while Task 1 is dominating CPU
    out->tasks[1].id = 2;
    strncpy(out->tasks[1].name, "Interactive_UI_Event_1", sizeof(out->tasks[1].name) - 1);
    out->tasks[1].type = TASK_TYPE_INTERACTIVE;
    out->tasks[1].base_priority = 1;
    out->tasks[1].target_duration_ms = 40.0 * scale_factor;
    out->tasks[1].total_iterations = (unsigned long long)(out->tasks[1].target_duration_ms * (double)iters_per_ms);
    out->tasks[1].arrival_offset_ms = 150.0 * scale_factor;
    strncpy(out->tasks[1].description, "Latency-sensitive user interface redraw", sizeof(out->tasks[1].description) - 1);

    // Task 3: Background Memory Stride job starting at t=0
    out->tasks[2].id = 3;
    strncpy(out->tasks[2].name, "BG_Memory_Stride_2", sizeof(out->tasks[2].name) - 1);
    out->tasks[2].type = TASK_TYPE_BACKGROUND_MEM;
    out->tasks[2].base_priority = 5;
    out->tasks[2].target_duration_ms = 500.0 * scale_factor;
    out->tasks[2].total_iterations = (unsigned long long)(out->tasks[2].target_duration_ms * (double)iters_per_ms);
    out->tasks[2].arrival_offset_ms = 0.0;
    strncpy(out->tasks[2].description, "Memory-intensive cache-line stride pass", sizeof(out->tasks[2].description) - 1);

    // Task 4: Short Interactive job arriving at t=350ms
    out->tasks[3].id = 4;
    strncpy(out->tasks[3].name, "Interactive_Button_Click_2", sizeof(out->tasks[3].name) - 1);
    out->tasks[3].type = TASK_TYPE_INTERACTIVE;
    out->tasks[3].base_priority = 1;
    out->tasks[3].target_duration_ms = 35.0 * scale_factor;
    out->tasks[3].total_iterations = (unsigned long long)(out->tasks[3].target_duration_ms * (double)iters_per_ms);
    out->tasks[3].arrival_offset_ms = 350.0 * scale_factor;
    strncpy(out->tasks[3].description, "Interactive user button click handler", sizeof(out->tasks[3].description) - 1);

    // Task 5: Short Interactive job arriving at t=550ms
    out->tasks[4].id = 5;
    strncpy(out->tasks[4].name, "Interactive_KeyPress_3", sizeof(out->tasks[4].name) - 1);
    out->tasks[4].type = TASK_TYPE_INTERACTIVE;
    out->tasks[4].base_priority = 1;
    out->tasks[4].target_duration_ms = 45.0 * scale_factor;
    out->tasks[4].total_iterations = (unsigned long long)(out->tasks[4].target_duration_ms * (double)iters_per_ms);
    out->tasks[4].arrival_offset_ms = 550.0 * scale_factor;
    strncpy(out->tasks[4].description, "Immediate user keystroke processing", sizeof(out->tasks[4].description) - 1);

    // Task 6: Heavy background compute job starting at t=0
    out->tasks[5].id = 6;
    strncpy(out->tasks[5].name, "BG_Batch_Crunch_3", sizeof(out->tasks[5].name) - 1);
    out->tasks[5].type = TASK_TYPE_BACKGROUND_CPU;
    out->tasks[5].base_priority = 5;
    out->tasks[5].target_duration_ms = 650.0 * scale_factor;
    out->tasks[5].total_iterations = (unsigned long long)(out->tasks[5].target_duration_ms * (double)iters_per_ms);
    out->tasks[5].arrival_offset_ms = 0.0;
    strncpy(out->tasks[5].description, "Prolonged background data analytics crunch", sizeof(out->tasks[5].description) - 1);

    out->task_count = 6;
    out->total_work_ms = 0.0;
    out->background_count = 3;
    out->interactive_count = 3;

    for (int i = 0; i < out->task_count; i++) {
        out->total_work_ms += out->tasks[i].target_duration_ms;
    }
}

void workload_generate_custom(WorkloadBatch* out, int num_bg, int num_interactive, double bg_duration_ms, double interactive_duration_ms) {
    if (!out) return;
    memset(out, 0, sizeof(WorkloadBatch));
    strncpy(out->profile_name, "Custom Tuned Workload", sizeof(out->profile_name) - 1);

    const MachineCalibration* cal = calibration_get();
    unsigned long long iters_per_ms = cal->iterations_per_ms;

    if (num_bg < 1) num_bg = 1;
    if (num_bg > 6) num_bg = 6;
    if (num_interactive < 1) num_interactive = 1;
    if (num_interactive > 6) num_interactive = 6;

    int idx = 0;
    // Add background jobs
    for (int i = 0; i < num_bg && idx < MAX_WORKLOAD_TASKS; i++) {
        out->tasks[idx].id = idx + 1;
        snprintf(out->tasks[idx].name, sizeof(out->tasks[idx].name), "BG_Worker_%d", i + 1);
        out->tasks[idx].type = (i % 2 == 0) ? TASK_TYPE_BACKGROUND_CPU : TASK_TYPE_BACKGROUND_MEM;
        out->tasks[idx].base_priority = 5;
        out->tasks[idx].target_duration_ms = bg_duration_ms;
        out->tasks[idx].total_iterations = (unsigned long long)(bg_duration_ms * (double)iters_per_ms);
        out->tasks[idx].arrival_offset_ms = 0.0;
        snprintf(out->tasks[idx].description, sizeof(out->tasks[idx].description), "Background Task %d", i + 1);
        idx++;
    }

    // Add interactive jobs distributed across the execution span
    double spacing = (bg_duration_ms * (double)num_bg * 0.7) / (double)(num_interactive + 1);
    for (int i = 0; i < num_interactive && idx < MAX_WORKLOAD_TASKS; i++) {
        out->tasks[idx].id = idx + 1;
        snprintf(out->tasks[idx].name, sizeof(out->tasks[idx].name), "Interactive_%d", i + 1);
        out->tasks[idx].type = TASK_TYPE_INTERACTIVE;
        out->tasks[idx].base_priority = 1;
        out->tasks[idx].target_duration_ms = interactive_duration_ms;
        out->tasks[idx].total_iterations = (unsigned long long)(interactive_duration_ms * (double)iters_per_ms);
        out->tasks[idx].arrival_offset_ms = spacing * (double)(i + 1);
        snprintf(out->tasks[idx].description, sizeof(out->tasks[idx].description), "Interactive Task %d", i + 1);
        idx++;
    }

    out->task_count = idx;
    out->background_count = num_bg;
    out->interactive_count = num_interactive;
    out->total_work_ms = 0.0;
    for (int i = 0; i < out->task_count; i++) {
        out->total_work_ms += out->tasks[i].target_duration_ms;
    }
}
