#include "scheduler_adaptive.h"
#include "calibration.h"
#include "system_telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static DWORD WINAPI adaptive_worker_thread(LPVOID lpParam) {
    ManagedWorker* w = (ManagedWorker*)lpParam;
    if (w->target_affinity > 0) {
        SetThreadAffinityMask(GetCurrentThread(), w->target_affinity);
    }

    while (InterlockedCompareExchange(&w->terminate_flag, 0, 0) == 0 && w->remaining_iterations > 0) {
        WaitForSingleObject(w->h_run_event, INFINITE);
        if (InterlockedCompareExchange(&w->terminate_flag, 0, 0) != 0) break;

        // Reset yield flag before starting slice
        InterlockedExchange(&w->yield_flag, 0);

        unsigned long long done = 0;
        if (w->def.type == TASK_TYPE_BACKGROUND_MEM) {
            done = execute_memory_block(w->remaining_iterations, &w->yield_flag);
        } else {
            done = execute_compute_block(w->remaining_iterations, &w->yield_flag);
        }

        w->completed_iterations += done;
        if (done <= w->remaining_iterations) {
            w->remaining_iterations -= done;
        } else {
            w->remaining_iterations = 0;
        }

        if (w->remaining_iterations == 0) {
            SetEvent(w->h_completed_event);
            break;
        } else {
            // Preemption checkpoint reached
            SetEvent(w->h_yielded_event);
        }
    }
    return 0;
}

static void log_transition(AlgorithmRunResult* res, double now_ms, int task_id, const char* name, TaskExecutionState from, TaskExecutionState to, const char* trigger) {
    if (res->transition_count < MAX_TRANSITIONS_LOG) {
        TaskTransition* tr = &res->transitions[res->transition_count++];
        tr->timestamp_ms = now_ms;
        tr->task_id = task_id;
        safe_copy_str(tr->task_name, name, sizeof(tr->task_name));
        tr->from_state = from;
        tr->to_state = to;
        safe_copy_str(tr->trigger_reason, trigger, sizeof(tr->trigger_reason));
    }
}

BOOL run_adaptive_benchmark(const WorkloadBatch* batch, DWORD_PTR affinity_mask, AlgorithmRunResult* out, AdaptivePolicyContext* out_ctx, volatile LONG* cancel_flag) {
    if (!batch || !out || batch->task_count <= 0) return FALSE;
    memset(out, 0, sizeof(AlgorithmRunResult));
    strncpy(out->algorithm_name, "Adaptive Priority with Aging & Preemption", sizeof(out->algorithm_name) - 1);
    strncpy(out->algorithm_code, "ADAPTIVE", sizeof(out->algorithm_code) - 1);
    out->task_count = batch->task_count;

    // Incorporate genuine host telemetry
    double host_cpu = telemetry_get_instant_cpu();
    int aging_thresh = 150;
    if (host_cpu > 50.0) {
        aging_thresh = 120; // Accelerate aging under high system load
    }
    if (out_ctx) {
        out_ctx->host_cpu_load_percent = host_cpu;
        out_ctx->aging_threshold_ms = aging_thresh;
        out_ctx->checkpoint_interval_ms = 15.0;
        if (host_cpu > 50.0) {
            snprintf(out_ctx->adaptive_reasoning, sizeof(out_ctx->adaptive_reasoning),
                     "Host CPU utilization is elevated (%.1f%%). Aging threshold set to %dms to accelerate background throughput while maintaining cooperative preemption for interactive bursts.",
                     host_cpu, aging_thresh);
        } else {
            snprintf(out_ctx->adaptive_reasoning, sizeof(out_ctx->adaptive_reasoning),
                     "Host system operates under nominal load (%.1f%%). Standard %dms aging threshold applied with ~15ms cooperative checkpoints to eliminate convoy latency.",
                     host_cpu, aging_thresh);
        }
    }

    LARGE_INTEGER t_bench_start, t_now;
    QueryPerformanceCounter(&t_bench_start);

    // Initialize workers
    for (int i = 0; i < batch->task_count; i++) {
        ManagedWorker* w = &out->workers[i];
        w->def = batch->tasks[i];
        w->current_state = STATE_NEW;
        w->effective_priority = batch->tasks[i].base_priority;
        w->remaining_iterations = batch->tasks[i].total_iterations;
        w->completed_iterations = 0;
        w->actual_arrival_ms = -1.0;
        w->first_dispatch_ms = -1.0;
        w->completion_ms = -1.0;
        w->total_cpu_time_ms = 0.0;
        w->waiting_time_ms = 0.0;
        w->turnaround_time_ms = 0.0;
        w->response_time_ms = 0.0;
        w->times_preempted = 0;
        w->times_dispatched = 0;
        w->yield_flag = 0;
        w->terminate_flag = 0;
        w->target_affinity = affinity_mask;

        w->h_run_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        w->h_yielded_event = CreateEvent(NULL, FALSE, FALSE, NULL);
        w->h_completed_event = CreateEvent(NULL, FALSE, FALSE, NULL);

        w->h_thread = CreateThread(NULL, 0, adaptive_worker_thread, w, 0, NULL);
    }

    // Ready Queue tracking
    typedef struct {
        int worker_idx;
        double entered_ready_ms;
    } ReadyEntry;

    ReadyEntry ready_queue[MAX_WORKLOAD_TASKS];
    int q_size = 0;
    int completed_tasks = 0;
    int last_dispatched_id = -1;

    // Benchmark loop
    while (completed_tasks < batch->task_count) {
        if (cancel_flag && InterlockedCompareExchange(cancel_flag, 0, 0) != 0) {
            break;
        }

        QueryPerformanceCounter(&t_now);
        double elapsed_ms = qpc_diff_ms(t_bench_start, t_now);

        // 1. Check for newly arriving tasks
        for (int i = 0; i < batch->task_count; i++) {
            ManagedWorker* w = &out->workers[i];
            if (w->current_state == STATE_NEW && elapsed_ms >= w->def.arrival_offset_ms) {
                w->current_state = STATE_READY;
                w->actual_arrival_ms = elapsed_ms;
                log_transition(out, elapsed_ms, w->def.id, w->def.name, STATE_NEW, STATE_READY, "Arrival");
                ready_queue[q_size].worker_idx = i;
                ready_queue[q_size].entered_ready_ms = elapsed_ms;
                q_size++;
            }
        }

        if (q_size == 0) {
            Sleep(1);
            continue;
        }

        // 2. Apply Aging mechanism to all waiting tasks in Ready Queue
        QueryPerformanceCounter(&t_now);
        elapsed_ms = qpc_diff_ms(t_bench_start, t_now);
        for (int k = 0; k < q_size; k++) {
            ManagedWorker* w = &out->workers[ready_queue[k].worker_idx];
            double wait_duration = elapsed_ms - ready_queue[k].entered_ready_ms;
            int aging_bonus = (int)(wait_duration / (double)aging_thresh);
            w->effective_priority = w->def.base_priority - aging_bonus;
            if (w->effective_priority < 0) w->effective_priority = 0; // Clamped at 0 (maximum priority)
        }

        // 3. Select best task: Lowest effective_priority number (highest priority)
        // Tie-breaker: Earliest arrival offset
        int best_q_pos = 0;
        int best_pri = out->workers[ready_queue[0].worker_idx].effective_priority;
        double best_arr = out->workers[ready_queue[0].worker_idx].def.arrival_offset_ms;

        for (int k = 1; k < q_size; k++) {
            ManagedWorker* cand = &out->workers[ready_queue[k].worker_idx];
            if (cand->effective_priority < best_pri) {
                best_pri = cand->effective_priority;
                best_arr = cand->def.arrival_offset_ms;
                best_q_pos = k;
            } else if (cand->effective_priority == best_pri && cand->def.arrival_offset_ms < best_arr) {
                best_arr = cand->def.arrival_offset_ms;
                best_q_pos = k;
            }
        }

        int selected_idx = ready_queue[best_q_pos].worker_idx;
        // Remove from queue
        for (int k = best_q_pos; k < q_size - 1; k++) {
            ready_queue[k] = ready_queue[k + 1];
        }
        q_size--;

        ManagedWorker* sel_w = &out->workers[selected_idx];
        QueryPerformanceCounter(&t_now);
        double slice_start_ms = qpc_diff_ms(t_bench_start, t_now);

        if (sel_w->first_dispatch_ms < 0.0) {
            sel_w->first_dispatch_ms = slice_start_ms;
            sel_w->response_time_ms = sel_w->first_dispatch_ms - sel_w->actual_arrival_ms;
            if (sel_w->response_time_ms < 0.0) sel_w->response_time_ms = 0.0;
        }

        if (last_dispatched_id != -1 && last_dispatched_id != sel_w->def.id) {
            out->total_context_switches++;
        }
        last_dispatched_id = sel_w->def.id;

        sel_w->current_state = STATE_RUNNING;
        sel_w->times_dispatched++;
        char disp_reason[64];
        snprintf(disp_reason, sizeof(disp_reason), "Dispatched (Priority %d)", sel_w->effective_priority);
        log_transition(out, slice_start_ms, sel_w->def.id, sel_w->def.name, STATE_READY, STATE_RUNNING, disp_reason);

        // 4. Dispatch worker with cooperative preemption monitoring
        SetEvent(sel_w->h_run_event);

        HANDLE wait_events[2] = { sel_w->h_completed_event, sel_w->h_yielded_event };
        BOOL was_preempted = FALSE;

        while (TRUE) {
            // Check for completion or yield with 5ms check intervals
            DWORD dwWait = WaitForMultipleObjects(2, wait_events, FALSE, 5);
            QueryPerformanceCounter(&t_now);
            elapsed_ms = qpc_diff_ms(t_bench_start, t_now);

            // Check if any higher-priority tasks have arrived
            for (int i = 0; i < batch->task_count; i++) {
                ManagedWorker* w = &out->workers[i];
                if (w->current_state == STATE_NEW && elapsed_ms >= w->def.arrival_offset_ms) {
                    w->current_state = STATE_READY;
                    w->actual_arrival_ms = elapsed_ms;
                    log_transition(out, elapsed_ms, w->def.id, w->def.name, STATE_NEW, STATE_READY, "Arrival");
                    ready_queue[q_size].worker_idx = i;
                    ready_queue[q_size].entered_ready_ms = elapsed_ms;
                    q_size++;

                    // If newly arrived task has higher priority than currently running task, signal preemption!
                    if (w->effective_priority < sel_w->effective_priority) {
                        InterlockedExchange(&sel_w->yield_flag, 1);
                    }
                }
            }

            if (dwWait == WAIT_OBJECT_0) {
                // Completed
                break;
            } else if (dwWait == WAIT_OBJECT_0 + 1) {
                // Yielded at preemption checkpoint
                was_preempted = TRUE;
                break;
            }

            if (cancel_flag && InterlockedCompareExchange(cancel_flag, 0, 0) != 0) {
                break;
            }
        }

        QueryPerformanceCounter(&t_now);
        double slice_end_ms = qpc_diff_ms(t_bench_start, t_now);
        double duration = slice_end_ms - slice_start_ms;
        sel_w->total_cpu_time_ms += duration;

        // Record slice
        if (out->slice_count < MAX_TIMELINE_EVENTS) {
            ExecutionSlice* sl = &out->slices[out->slice_count++];
            sl->task_id = sel_w->def.id;
            strncpy(sl->task_name, sel_w->def.name, sizeof(sl->task_name) - 1);
            sl->type = sel_w->def.type;
            sl->start_ms = slice_start_ms;
            sl->end_ms = slice_end_ms;
            sl->duration_ms = duration;
            sl->was_preempted = was_preempted ? 1 : 0;
        }

        if (was_preempted && sel_w->remaining_iterations > 0) {
            sel_w->current_state = STATE_READY;
            sel_w->times_preempted++;
            out->total_preemptions++;
            log_transition(out, slice_end_ms, sel_w->def.id, sel_w->def.name, STATE_RUNNING, STATE_READY, "Cooperative Yield (Preempted)");

            // Re-enqueue preempted worker back into ready queue with its current timestamp
            ready_queue[q_size].worker_idx = selected_idx;
            ready_queue[q_size].entered_ready_ms = slice_end_ms;
            q_size++;
        } else {
            sel_w->current_state = STATE_COMPLETED;
            sel_w->completion_ms = slice_end_ms;
            sel_w->turnaround_time_ms = sel_w->completion_ms - sel_w->actual_arrival_ms;
            sel_w->waiting_time_ms = sel_w->turnaround_time_ms - sel_w->total_cpu_time_ms;
            if (sel_w->waiting_time_ms < 0.0) sel_w->waiting_time_ms = 0.0;
            log_transition(out, slice_end_ms, sel_w->def.id, sel_w->def.name, STATE_RUNNING, STATE_COMPLETED, "Completed");
            completed_tasks++;
        }
    }

    LARGE_INTEGER t_bench_end;
    QueryPerformanceCounter(&t_bench_end);
    out->total_benchmark_duration_ms = qpc_diff_ms(t_bench_start, t_bench_end);

    // Clean up worker threads
    for (int i = 0; i < batch->task_count; i++) {
        ManagedWorker* w = &out->workers[i];
        InterlockedExchange(&w->terminate_flag, 1);
        SetEvent(w->h_run_event);
    }
    for (int i = 0; i < batch->task_count; i++) {
        WaitForSingleObject(out->workers[i].h_thread, 1000);
        CloseHandle(out->workers[i].h_thread);
        CloseHandle(out->workers[i].h_run_event);
        CloseHandle(out->workers[i].h_yielded_event);
        CloseHandle(out->workers[i].h_completed_event);
    }

    // Compute aggregated metrics
    double sum_inter_resp = 0.0;
    double sum_inter_wait = 0.0;
    double sum_bg_tat = 0.0;
    int inter_cnt = 0;
    int bg_cnt = 0;
    double max_bg_completion = 0.0;
    double inter_resp_times[MAX_WORKLOAD_TASKS];

    for (int i = 0; i < batch->task_count; i++) {
        ManagedWorker* w = &out->workers[i];
        if (w->def.type == TASK_TYPE_INTERACTIVE) {
            sum_inter_resp += w->response_time_ms;
            sum_inter_wait += w->waiting_time_ms;
            inter_resp_times[inter_cnt] = w->response_time_ms;
            inter_cnt++;
        } else {
            sum_bg_tat += w->turnaround_time_ms;
            bg_cnt++;
            if (w->completion_ms > max_bg_completion) {
                max_bg_completion = w->completion_ms;
            }
        }
    }

    out->avg_interactive_response_time_ms = (inter_cnt > 0) ? (sum_inter_resp / (double)inter_cnt) : 0.0;
    out->avg_interactive_waiting_time_ms = (inter_cnt > 0) ? (sum_inter_wait / (double)inter_cnt) : 0.0;
    out->avg_background_turnaround_time_ms = (bg_cnt > 0) ? (sum_bg_tat / (double)bg_cnt) : 0.0;
    out->background_makespan_ms = max_bg_completion;

    if (inter_cnt > 0) {
        for (int i = 0; i < inter_cnt - 1; i++) {
            for (int j = 0; j < inter_cnt - i - 1; j++) {
                if (inter_resp_times[j] > inter_resp_times[j + 1]) {
                    double tmp = inter_resp_times[j];
                    inter_resp_times[j] = inter_resp_times[j + 1];
                    inter_resp_times[j + 1] = tmp;
                }
            }
        }
        int p95_idx = (int)((double)inter_cnt * 0.95);
        if (p95_idx >= inter_cnt) p95_idx = inter_cnt - 1;
        out->p95_interactive_response_time_ms = inter_resp_times[p95_idx];
    }

    return TRUE;
}
