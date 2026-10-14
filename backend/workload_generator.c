#include "workload_generator.h"
#include "calibration.h"
#include "system_telemetry.h"
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

void workload_prepare_lab(WorkloadBatch* out, const char* mode, const char* intensity) {
    if (!out) return;
    memset(out, 0, sizeof(WorkloadBatch));

    const MachineCalibration* cal = calibration_get();
    unsigned long long iters_per_ms = cal->iterations_per_ms;
    if (iters_per_ms == 0) iters_per_ms = 1000000;

    double b1_ms = 2400.0;
    double b2_ms = 1800.0;
    double i1_ms = 35.0;
    double i1_arr = 500.0;
    BOOL is_starvation_scenario = FALSE;

    if (intensity && (strcmp(intensity, "starvation") == 0 || strcmp(intensity, "anti_starvation") == 0)) {
        is_starvation_scenario = TRUE;
        b1_ms = 2000.0;
        b2_ms = 1400.0;
        strncpy(out->profile_name, "Anti-Starvation Validation (Burst Stream)", sizeof(out->profile_name) - 1);
    } else if (intensity && strcmp(intensity, "light") == 0) {
        b1_ms = 800.0;
        b2_ms = 650.0;
        i1_ms = 25.0;
        i1_arr = 200.0;
        strncpy(out->profile_name, "Light Lab Workload (~1.5s)", sizeof(out->profile_name) - 1);
    } else if (intensity && strcmp(intensity, "heavy") == 0) {
        b1_ms = 3500.0;
        b2_ms = 2800.0;
        i1_ms = 45.0;
        i1_arr = 700.0;
        strncpy(out->profile_name, "Heavy Lab Workload (~6.5s)", sizeof(out->profile_name) - 1);
    } else {
        // Balanced Classroom Demo (recommended for demonstration so user can observe queue & click urgent button comfortably)
        strncpy(out->profile_name, "Classroom Demo Workload (~4.5s)", sizeof(out->profile_name) - 1);
    }

    // Host telemetry calibration grounding
    char b1_desc[128] = "Long CPU-bound arithmetic loop (Matrix)";
    char b2_desc[128] = "Long memory-stride cache pressure worker";

    if (mode && strcmp(mode, "captured") == 0) {
        SystemCaptureResult cap;
        telemetry_get_capture_status(&cap);
        double host_cpu = cap.is_complete ? cap.avg_cpu_percent : telemetry_get_instant_cpu();
        int observed_procs = cap.is_complete ? cap.total_processes_observed : 0;
        int aging_val = cap.is_complete ? cap.recommended_aging_threshold_ms : ((host_cpu > 35.0) ? 120 : 150);

        // Under higher background load, scale compute duration to reflect external contention
        double load_scale = 1.0 + (host_cpu / 100.0) * 0.35;
        b1_ms *= load_scale;
        b2_ms *= load_scale;
        if (!is_starvation_scenario) {
            i1_arr = b1_ms * 0.25; // Mathematically guaranteed collision with B1's prime execution window
        }

        const char* base_name = is_starvation_scenario ? "Anti-Starvation" :
                                (intensity && strcmp(intensity, "light") == 0) ? "Light" :
                                (intensity && strcmp(intensity, "heavy") == 0) ? "Heavy" : "Classroom Demo";

        if (cap.browser_process_count > 0) {
            snprintf(out->profile_name, sizeof(out->profile_name), "%s [%s: %d procs, %.1f%% CPU, Aging: %dms]",
                     base_name, cap.detected_browser_name, cap.browser_process_count, cap.browser_total_cpu, aging_val);
            snprintf(b1_desc, sizeof(b1_desc), "CPU matrix loop (scaled %.2fx for %.1f%% host CPU, %d %s procs)",
                     load_scale, host_cpu, cap.browser_process_count, cap.detected_browser_name);
        } else {
            snprintf(out->profile_name, sizeof(out->profile_name), "%s [Host: %.1f%% CPU, Aging: %dms]",
                     base_name, host_cpu, aging_val);
            snprintf(b1_desc, sizeof(b1_desc), "CPU matrix loop (scaled %.2fx for %.1f%% host CPU, %d procs)",
                     load_scale, host_cpu, observed_procs);
        }
        snprintf(b2_desc, sizeof(b2_desc), "Memory stride pass (scaled %.2fx for %.1f%% host CPU)",
                 load_scale, host_cpu);
    }

    if (is_starvation_scenario) {
        // Task 1: B1 (Matrix Compute)
        out->tasks[0].id = 1;
        strncpy(out->tasks[0].name, "B1_Matrix_Compute", sizeof(out->tasks[0].name) - 1);
        out->tasks[0].type = TASK_TYPE_BACKGROUND_CPU;
        out->tasks[0].base_priority = 5;
        out->tasks[0].target_duration_ms = b1_ms;
        out->tasks[0].total_iterations = (unsigned long long)(b1_ms * (double)iters_per_ms);
        out->tasks[0].arrival_offset_ms = 0.0;
        snprintf(out->tasks[0].description, sizeof(out->tasks[0].description), "%s", b1_desc);

        // Task 2: B2 (Memory Stride - Subject of Starvation Test)
        out->tasks[1].id = 2;
        strncpy(out->tasks[1].name, "B2_Memory_Stride", sizeof(out->tasks[1].name) - 1);
        out->tasks[1].type = TASK_TYPE_BACKGROUND_MEM;
        out->tasks[1].base_priority = 5;
        out->tasks[1].target_duration_ms = b2_ms;
        out->tasks[1].total_iterations = (unsigned long long)(b2_ms * (double)iters_per_ms);
        out->tasks[1].arrival_offset_ms = 0.0;
        snprintf(out->tasks[1].description, sizeof(out->tasks[1].description), "%s", b2_desc);

        // Stream of 5 Higher-Priority Interactive Bursts (Priority 1) arriving back-to-back
        for (int i = 0; i < 5; i++) {
            int tid = 3 + i;
            out->tasks[2 + i].id = tid;
            snprintf(out->tasks[2 + i].name, sizeof(out->tasks[2 + i].name), "I%d_Stream_Burst", i + 1);
            out->tasks[2 + i].type = TASK_TYPE_INTERACTIVE;
            out->tasks[2 + i].base_priority = 1;
            out->tasks[2 + i].target_duration_ms = 30.0;
            out->tasks[2 + i].total_iterations = (unsigned long long)(30.0 * (double)iters_per_ms);
            out->tasks[2 + i].arrival_offset_ms = 250.0 * (double)(i + 1); // 250ms, 500ms, 750ms, 1000ms, 1250ms
            snprintf(out->tasks[2 + i].description, sizeof(out->tasks[2 + i].description),
                     "Interactive burst %d testing priority aging against B2", i + 1);
        }

        out->task_count = 7;
        out->background_count = 2;
        out->interactive_count = 5;
        out->total_work_ms = b1_ms + b2_ms + (5 * 30.0);
    } else {
        // Standard 3-task layout: B1, B2, I1 (plus any user-injected task)
        // Task 1: B1 (Matrix Compute)
        out->tasks[0].id = 1;
        strncpy(out->tasks[0].name, "B1_Matrix_Compute", sizeof(out->tasks[0].name) - 1);
        out->tasks[0].type = TASK_TYPE_BACKGROUND_CPU;
        out->tasks[0].base_priority = 5;
        out->tasks[0].target_duration_ms = b1_ms;
        out->tasks[0].total_iterations = (unsigned long long)(b1_ms * (double)iters_per_ms);
        out->tasks[0].arrival_offset_ms = 0.0;
        snprintf(out->tasks[0].description, sizeof(out->tasks[0].description), "%s", b1_desc);

        // Task 2: B2 (Memory Stride)
        out->tasks[1].id = 2;
        strncpy(out->tasks[1].name, "B2_Memory_Stride", sizeof(out->tasks[1].name) - 1);
        out->tasks[1].type = TASK_TYPE_BACKGROUND_MEM;
        out->tasks[1].base_priority = 5;
        out->tasks[1].target_duration_ms = b2_ms;
        out->tasks[1].total_iterations = (unsigned long long)(b2_ms * (double)iters_per_ms);
        out->tasks[1].arrival_offset_ms = 0.0;
        snprintf(out->tasks[1].description, sizeof(out->tasks[1].description), "%s", b2_desc);

        // Task 3: I1 (Interactive Request)
        out->tasks[2].id = 3;
        strncpy(out->tasks[2].name, "I1_Interactive_UI", sizeof(out->tasks[2].name) - 1);
        out->tasks[2].type = TASK_TYPE_INTERACTIVE;
        out->tasks[2].base_priority = 1;
        out->tasks[2].target_duration_ms = i1_ms;
        out->tasks[2].total_iterations = (unsigned long long)(i1_ms * (double)iters_per_ms);
        out->tasks[2].arrival_offset_ms = i1_arr;
        snprintf(out->tasks[2].description, sizeof(out->tasks[2].description),
                 "Short user request arriving while B1 executes");

        out->task_count = 3;
        out->background_count = 2;
        out->interactive_count = 1;
        out->total_work_ms = b1_ms + b2_ms + i1_ms;
    }
}

int workload_inject_interactive_task(WorkloadBatch* batch, double arrival_offset_ms, const char* name) {
    if (!batch || batch->task_count >= MAX_WORKLOAD_TASKS) return -1;
    const MachineCalibration* cal = calibration_get();
    unsigned long long iters_per_ms = cal->iterations_per_ms;
    if (iters_per_ms == 0) iters_per_ms = 1000000;

    int new_id = batch->task_count + 1;
    ManagedTaskDef* t = &batch->tasks[batch->task_count];
    t->id = new_id;
    if (name && *name) {
        snprintf(t->name, sizeof(t->name), "%s", name);
    } else {
        snprintf(t->name, sizeof(t->name), "I%d_Urgent_Request", new_id);
    }
    t->type = TASK_TYPE_INTERACTIVE;
    t->base_priority = 1;
    t->target_duration_ms = 35.0;
    t->total_iterations = (unsigned long long)(t->target_duration_ms * (double)iters_per_ms);
    t->arrival_offset_ms = arrival_offset_ms;
    snprintf(t->description, sizeof(t->description), "Dynamically injected urgent user request");

    batch->task_count++;
    batch->interactive_count++;
    batch->total_work_ms += t->target_duration_ms;
    return new_id;
}

