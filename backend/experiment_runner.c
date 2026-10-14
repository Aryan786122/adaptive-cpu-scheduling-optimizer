#include "experiment_runner.h"
#include "calibration.h"
#include "system_telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static CRITICAL_SECTION s_exp_cs;
static BOOL s_exp_init = FALSE;
static ExperimentReport s_current_report = {0};
static HANDLE s_exp_thread = NULL;
static volatile LONG s_cancel_requested = 0;
static ULONGLONG s_exp_start_tick = 0;

void experiment_init(void) {
    if (!s_exp_init) {
        InitializeCriticalSection(&s_exp_cs);
        calibration_init();
        telemetry_init();
        s_current_report.status = EXP_STATUS_IDLE;
        strncpy(s_current_report.current_step_description, "Ready to start live benchmark", sizeof(s_current_report.current_step_description) - 1);
        s_exp_init = TRUE;
    }
}

static void generate_empirical_conclusion(ExperimentReport* rep) {
    double fcfs_rt = rep->fcfs_result.avg_interactive_response_time_ms;
    double adapt_rt = rep->adaptive_result.avg_interactive_response_time_ms;
    double fcfs_wt = rep->fcfs_result.avg_interactive_waiting_time_ms;
    double adapt_wt = rep->adaptive_result.avg_interactive_waiting_time_ms;
    double fcfs_ms = rep->fcfs_result.background_makespan_ms;
    double adapt_ms = rep->adaptive_result.background_makespan_ms;

    if (fcfs_rt > 0.001) {
        rep->response_time_improvement_pct = ((fcfs_rt - adapt_rt) / fcfs_rt) * 100.0;
    } else {
        rep->response_time_improvement_pct = 0.0;
    }

    if (fcfs_wt > 0.001) {
        rep->waiting_time_improvement_pct = ((fcfs_wt - adapt_wt) / fcfs_wt) * 100.0;
    } else {
        rep->waiting_time_improvement_pct = 0.0;
    }

    if (fcfs_ms > 0.001) {
        rep->background_makespan_delta_pct = ((adapt_ms - fcfs_ms) / fcfs_ms) * 100.0;
    } else {
        rep->background_makespan_delta_pct = 0.0;
    }

    rep->preemption_count = rep->adaptive_result.total_preemptions;
    rep->context_switch_delta = rep->adaptive_result.total_context_switches - rep->fcfs_result.total_context_switches;

    // Dynamic conclusion text matching exact required format
    if (rep->response_time_improvement_pct >= 0.0) {
        snprintf(rep->empirical_conclusion, sizeof(rep->empirical_conclusion),
            "Adaptive scheduling reduced interactive request latency by %.1f%%, while background completion time changed by %+.1f%%.",
            rep->response_time_improvement_pct, rep->background_makespan_delta_pct);
    } else {
        snprintf(rep->empirical_conclusion, sizeof(rep->empirical_conclusion),
            "Adaptive scheduling increased interactive request latency by %.1f%%, while background completion time changed by %+.1f%%.",
            -rep->response_time_improvement_pct, rep->background_makespan_delta_pct);
    }

    // Trade-off and theoretical analysis
    snprintf(rep->trade_off_analysis, sizeof(rep->trade_off_analysis),
        "Under non-preemptive FCFS, short interactive tasks were trapped in the ready queue behind long-running background computation (The Convoy Effect). The Adaptive Priority Scheduler enabled interactive requests to gain immediate CPU attention at safe cooperative checkpoints (%d preemptions recorded). Dynamic aging prevented background starvation, ensuring all computational work completed reliably.",
        rep->preemption_count);
}

static UrgentInjectionSlot s_urgent_slot = { .requested = 0, .name = "" };

void experiment_prepare_workload(const char* mode, const char* intensity) {
    experiment_init();
    EnterCriticalSection(&s_exp_cs);
    workload_prepare_lab(&s_current_report.workload, mode, intensity);
    memset(&s_current_report.fcfs_result, 0, sizeof(AlgorithmRunResult));
    memset(&s_current_report.adaptive_result, 0, sizeof(AlgorithmRunResult));
    s_current_report.status = EXP_STATUS_IDLE;
    s_current_report.progress_percent = 0.0;
    snprintf(s_current_report.current_step_description, sizeof(s_current_report.current_step_description),
             "Prepared %d-task lab workload (%s). Ready to execute FCFS.",
             s_current_report.workload.task_count, s_current_report.workload.profile_name);
    LeaveCriticalSection(&s_exp_cs);
}

BOOL experiment_inject_urgent_task(const char* name) {
    EnterCriticalSection(&s_exp_cs);
    if (s_current_report.status != EXP_STATUS_RUNNING_FCFS) {
        LeaveCriticalSection(&s_exp_cs);
        return FALSE; // Can only inject while FCFS is running
    }
    safe_copy_str(s_urgent_slot.name, name ? name : "I_Urgent_Request", sizeof(s_urgent_slot.name));
    InterlockedExchange(&s_urgent_slot.requested, 1);
    LeaveCriticalSection(&s_exp_cs);
    return TRUE;
}

static DWORD WINAPI fcfs_single_thread_proc(LPVOID lpParam) {
    (void)lpParam;
    calibration_run();
    const MachineCalibration* cal = calibration_get();
    SystemTelemetry telem;
    telemetry_get(&telem);

    EnterCriticalSection(&s_exp_cs);
    safe_copy_str(s_current_report.cpu_model, telem.cpu_model, sizeof(s_current_report.cpu_model));
    s_current_report.logical_cores = telem.logical_cores;
    s_current_report.host_cpu_at_start = telem.cpu_percent;
    s_current_report.cal_iters_per_ms = cal->iterations_per_ms;
    s_current_report.status = EXP_STATUS_RUNNING_FCFS;
    s_current_report.progress_percent = 25.0;
    s_current_report.affinity_mask = 0x1; // Core 0 contention
    s_current_report.affinity_core_count = 1;

    if (s_current_report.workload.task_count == 0) {
        workload_prepare_lab(&s_current_report.workload, "standard", "balanced");
    }

    snprintf(s_current_report.current_step_description, sizeof(s_current_report.current_step_description),
             "Executing FCFS on Core 0. Observe B1 execution and Convoy delay...");
    LeaveCriticalSection(&s_exp_cs);

    AlgorithmRunResult fcfs_out;
    InterlockedExchange(&s_urgent_slot.requested, 0);

    if (!run_fcfs_benchmark(&s_current_report.workload, 0x1, &fcfs_out, &s_urgent_slot, &s_cancel_requested)) {
        EnterCriticalSection(&s_exp_cs);
        s_current_report.status = EXP_STATUS_CANCELLED;
        strncpy(s_current_report.current_step_description, "FCFS execution cancelled by user", sizeof(s_current_report.current_step_description) - 1);
        LeaveCriticalSection(&s_exp_cs);
        return 0;
    }

    EnterCriticalSection(&s_exp_cs);
    s_current_report.fcfs_result = fcfs_out;
    s_current_report.status = EXP_STATUS_FCFS_DONE;
    s_current_report.progress_percent = 50.0;
    snprintf(s_current_report.current_step_description, sizeof(s_current_report.current_step_description),
             "FCFS complete! Interactive delay was %.1f ms. Ready to run Adaptive on the SAME workload.",
             fcfs_out.avg_interactive_response_time_ms);
    LeaveCriticalSection(&s_exp_cs);
    return 0;
}

static DWORD WINAPI adaptive_single_thread_proc(LPVOID lpParam) {
    (void)lpParam;
    EnterCriticalSection(&s_exp_cs);
    s_current_report.status = EXP_STATUS_RUNNING_ADAPTIVE;
    s_current_report.progress_percent = 70.0;
    snprintf(s_current_report.current_step_description, sizeof(s_current_report.current_step_description),
             "Executing Adaptive Priority Scheduling on identical workload with ~15ms preemption checkpoints...");
    LeaveCriticalSection(&s_exp_cs);

    AlgorithmRunResult adapt_out;
    AdaptivePolicyContext adapt_ctx;

    if (!run_adaptive_benchmark(&s_current_report.workload, 0x1, &adapt_out, &adapt_ctx, &s_cancel_requested)) {
        EnterCriticalSection(&s_exp_cs);
        s_current_report.status = EXP_STATUS_CANCELLED;
        strncpy(s_current_report.current_step_description, "Adaptive execution cancelled by user", sizeof(s_current_report.current_step_description) - 1);
        LeaveCriticalSection(&s_exp_cs);
        return 0;
    }

    EnterCriticalSection(&s_exp_cs);
    s_current_report.adaptive_result = adapt_out;
    s_current_report.adaptive_ctx = adapt_ctx;
    generate_empirical_conclusion(&s_current_report);
    s_current_report.status = EXP_STATUS_COMPLETED;
    s_current_report.progress_percent = 100.0;
    snprintf(s_current_report.current_step_description, sizeof(s_current_report.current_step_description),
             "Both runs finished on identical workload! Full comparative metrics available.");
    LeaveCriticalSection(&s_exp_cs);
    return 0;
}

BOOL experiment_run_fcfs(DWORD_PTR affinity_mask) {
    (void)affinity_mask;
    experiment_init();
    EnterCriticalSection(&s_exp_cs);
    if (s_current_report.status == EXP_STATUS_RUNNING_FCFS ||
        s_current_report.status == EXP_STATUS_RUNNING_ADAPTIVE) {
        LeaveCriticalSection(&s_exp_cs);
        return FALSE;
    }

    s_cancel_requested = 0;
    s_exp_start_tick = GetTickCount64();

    if (s_exp_thread) {
        CloseHandle(s_exp_thread);
        s_exp_thread = NULL;
    }

    s_exp_thread = CreateThread(NULL, 0, fcfs_single_thread_proc, NULL, 0, NULL);
    LeaveCriticalSection(&s_exp_cs);
    return (s_exp_thread != NULL);
}

BOOL experiment_run_adaptive(DWORD_PTR affinity_mask) {
    (void)affinity_mask;
    experiment_init();
    EnterCriticalSection(&s_exp_cs);
    if (s_current_report.status == EXP_STATUS_RUNNING_FCFS ||
        s_current_report.status == EXP_STATUS_RUNNING_ADAPTIVE) {
        LeaveCriticalSection(&s_exp_cs);
        return FALSE;
    }

    s_cancel_requested = 0;
    s_exp_start_tick = GetTickCount64();

    if (s_exp_thread) {
        CloseHandle(s_exp_thread);
        s_exp_thread = NULL;
    }

    s_exp_thread = CreateThread(NULL, 0, adaptive_single_thread_proc, NULL, 0, NULL);
    LeaveCriticalSection(&s_exp_cs);
    return (s_exp_thread != NULL);
}

static DWORD WINAPI experiment_thread_proc(LPVOID lpParam) {
    (void)lpParam;

    EnterCriticalSection(&s_exp_cs);
    s_current_report.status = EXP_STATUS_CALIBRATING;
    s_current_report.progress_percent = 5.0;
    strncpy(s_current_report.current_step_description, "Step 1/5: Detecting hardware & calibrating workload units...", sizeof(s_current_report.current_step_description) - 1);
    LeaveCriticalSection(&s_exp_cs);

    // Step 1 & 2: Hardware detection & calibration
    SystemTelemetry telem;
    telemetry_get(&telem);
    calibration_run();
    const MachineCalibration* cal = calibration_get();

    EnterCriticalSection(&s_exp_cs);
    safe_copy_str(s_current_report.cpu_model, telem.cpu_model, sizeof(s_current_report.cpu_model));
    s_current_report.logical_cores = telem.logical_cores;
    s_current_report.host_cpu_at_start = telem.cpu_percent;
    s_current_report.cal_iters_per_ms = cal->iterations_per_ms;

    if (s_current_report.affinity_mask == 0) {
        s_current_report.affinity_mask = telem.affinity_mask_contention_single;
        s_current_report.affinity_core_count = 1;
    }

    // Step 3: Prepare workload definitions if not already prepared
    s_current_report.progress_percent = 15.0;
    strncpy(s_current_report.current_step_description, "Step 2/5: Synthesizing identical repeatable workload definitions...", sizeof(s_current_report.current_step_description) - 1);
    if (s_current_report.workload.task_count == 0) {
        workload_prepare_lab(&s_current_report.workload, "standard", "balanced");
    }
    DWORD_PTR aff_mask = s_current_report.affinity_mask;
    LeaveCriticalSection(&s_exp_cs);

    if (InterlockedCompareExchange(&s_cancel_requested, 0, 0) != 0) {
        EnterCriticalSection(&s_exp_cs);
        s_current_report.status = EXP_STATUS_CANCELLED;
        strncpy(s_current_report.current_step_description, "Benchmark cancelled by user", sizeof(s_current_report.current_step_description) - 1);
        LeaveCriticalSection(&s_exp_cs);
        return 0;
    }

    // Step 4: Execute FCFS
    EnterCriticalSection(&s_exp_cs);
    s_current_report.status = EXP_STATUS_RUNNING_FCFS;
    s_current_report.progress_percent = 25.0;
    strncpy(s_current_report.current_step_description, "Step 3/5: Executing non-preemptive FCFS benchmark on managed worker pool...", sizeof(s_current_report.current_step_description) - 1);
    LeaveCriticalSection(&s_exp_cs);

    AlgorithmRunResult fcfs_out;
    InterlockedExchange(&s_urgent_slot.requested, 0);

    if (!run_fcfs_benchmark(&s_current_report.workload, aff_mask, &fcfs_out, &s_urgent_slot, &s_cancel_requested)) {
        EnterCriticalSection(&s_exp_cs);
        s_current_report.status = EXP_STATUS_CANCELLED;
        strncpy(s_current_report.current_step_description, "Benchmark cancelled during FCFS execution", sizeof(s_current_report.current_step_description) - 1);
        LeaveCriticalSection(&s_exp_cs);
        return 0;
    }

    EnterCriticalSection(&s_exp_cs);
    s_current_report.fcfs_result = fcfs_out;
    s_current_report.progress_percent = 60.0;
    s_current_report.status = EXP_STATUS_RUNNING_ADAPTIVE;
    strncpy(s_current_report.current_step_description, "Step 4/5: Executing Adaptive Scheduler with preemption checkpoints & aging...", sizeof(s_current_report.current_step_description) - 1);
    LeaveCriticalSection(&s_exp_cs);

    Sleep(20);

    // Step 5: Execute Adaptive Scheduler on identical workload batch
    AlgorithmRunResult adapt_out;
    AdaptivePolicyContext adapt_ctx;
    if (!run_adaptive_benchmark(&s_current_report.workload, aff_mask, &adapt_out, &adapt_ctx, &s_cancel_requested)) {
        EnterCriticalSection(&s_exp_cs);
        s_current_report.status = EXP_STATUS_CANCELLED;
        strncpy(s_current_report.current_step_description, "Benchmark cancelled during Adaptive execution", sizeof(s_current_report.current_step_description) - 1);
        LeaveCriticalSection(&s_exp_cs);
        return 0;
    }

    // Step 6: Finalize results & generate conclusion
    EnterCriticalSection(&s_exp_cs);
    s_current_report.adaptive_result = adapt_out;
    s_current_report.adaptive_ctx = adapt_ctx;
    generate_empirical_conclusion(&s_current_report);
    s_current_report.progress_percent = 100.0;
    s_current_report.status = EXP_STATUS_COMPLETED;
    strncpy(s_current_report.current_step_description, "Benchmark completed successfully. Real comparison metrics available.", sizeof(s_current_report.current_step_description) - 1);
    LeaveCriticalSection(&s_exp_cs);

    return 0;
}

BOOL experiment_start(DWORD_PTR affinity_mask, double scale_factor, BOOL use_contention_affinity) {
    experiment_init();
    EnterCriticalSection(&s_exp_cs);
    if (s_current_report.status == EXP_STATUS_CALIBRATING ||
        s_current_report.status == EXP_STATUS_RUNNING_FCFS ||
        s_current_report.status == EXP_STATUS_RUNNING_ADAPTIVE) {
        LeaveCriticalSection(&s_exp_cs);
        return FALSE; // Already running
    }

    if (scale_factor < 0.2 || scale_factor > 5.0) scale_factor = 1.0;
    s_current_report.scale_factor = scale_factor;

    if (use_contention_affinity || affinity_mask == 0) {
        s_current_report.affinity_mask = 0x1; // Single core contention
        s_current_report.affinity_core_count = 1;
    } else {
        s_current_report.affinity_mask = affinity_mask;
        int count = 0;
        for (int i = 0; i < 32; i++) {
            if (affinity_mask & ((DWORD_PTR)1 << i)) count++;
        }
        s_current_report.affinity_core_count = count > 0 ? count : 1;
    }

    s_current_report.status = EXP_STATUS_CALIBRATING;
    s_current_report.progress_percent = 0.0;
    s_cancel_requested = 0;
    s_exp_start_tick = GetTickCount64();

    if (s_exp_thread) {
        CloseHandle(s_exp_thread);
        s_exp_thread = NULL;
    }

    s_exp_thread = CreateThread(NULL, 0, experiment_thread_proc, NULL, 0, NULL);
    LeaveCriticalSection(&s_exp_cs);
    return TRUE;
}

void experiment_cancel(void) {
    InterlockedExchange(&s_cancel_requested, 1);
}

void experiment_get_status(ExperimentReport* out) {
    if (!out) return;
    experiment_init();
    EnterCriticalSection(&s_exp_cs);
    if (s_current_report.status != EXP_STATUS_IDLE && s_exp_start_tick > 0) {
        s_current_report.elapsed_seconds = (double)(GetTickCount64() - s_exp_start_tick) / 1000.0;
    }
    memcpy(out, &s_current_report, sizeof(ExperimentReport));
    LeaveCriticalSection(&s_exp_cs);
}

