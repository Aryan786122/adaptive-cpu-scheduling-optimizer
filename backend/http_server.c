#include "http_server.h"
#include "system_telemetry.h"
#include "experiment_runner.h"
#include "real_browser_benchmark.h"
#include "json_builder.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

static SOCKET s_listen_sock = INVALID_SOCKET;
static BOOL s_running = FALSE;
static int s_bound_port = 0;
static char s_web_dir[MAX_PATH * 2] = "";

int http_server_get_port(void) {
    return s_bound_port;
}

static void resolve_web_root(const char* override_dir) {
    if (override_dir && *override_dir) {
        strncpy(s_web_dir, override_dir, sizeof(s_web_dir) - 1);
        return;
    }

    // Locate relative to executable path
    char exe_path[MAX_PATH];
    if (GetModuleFileNameA(NULL, exe_path, sizeof(exe_path))) {
        char* last_slash = strrchr(exe_path, '\\');
        if (last_slash) {
            *last_slash = '\0';
            snprintf(s_web_dir, sizeof(s_web_dir), "%s\\frontend", exe_path);
            if (GetFileAttributesA(s_web_dir) != INVALID_FILE_ATTRIBUTES) {
                return;
            }
        }
    }

    // Fallback: Current working directory
    strncpy(s_web_dir, "frontend", sizeof(s_web_dir) - 1);
}

static void send_response(SOCKET sock, int status_code, const char* content_type, const char* body, size_t body_len) {
    char header[1024];
    const char* reason = (status_code == 200) ? "OK" : (status_code == 404 ? "Not Found" : "Bad Request");

    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d %s\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\n"
        "Connection: close\r\n"
        "\r\n",
        status_code, reason, content_type, body_len);

    send(sock, header, header_len, 0);
    if (body && body_len > 0) {
        send(sock, body, (int)body_len, 0);
    }
}

static void serve_file(SOCKET sock, const char* filename, const char* ctype) {
    char full[MAX_PATH * 4];
    snprintf(full, sizeof(full), "%s\\%s", s_web_dir, filename);

    FILE* f = fopen(full, "rb");
    if (!f) {
        const char* err = "404 Not Found";
        send_response(sock, 404, "text/plain", err, strlen(err));
        return;
    }

    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (sz <= 0) {
        fclose(f);
        send_response(sock, 200, ctype, "", 0);
        return;
    }

    char* buf = (char*)malloc(sz);
    if (buf) {
        fread(buf, 1, sz, f);
        fclose(f);
        send_response(sock, 200, ctype, buf, sz);
        free(buf);
    } else {
        fclose(f);
        send_response(sock, 500, "text/plain", "Memory allocation error", 23);
    }
}

static void handle_api_telemetry(SOCKET sock) {
    SystemTelemetry t;
    telemetry_get(&t);

    JsonBuilder jb;
    json_init(&jb, 4096);
    json_raw(&jb, "{");

    json_str(&jb, "hostname", t.hostname); json_raw(&jb, ",");
    json_str(&jb, "os_name", t.os_name); json_raw(&jb, ",");
    json_str(&jb, "architecture", t.architecture); json_raw(&jb, ",");
    json_str(&jb, "cpu_model", t.cpu_model); json_raw(&jb, ",");
    json_int(&jb, "physical_cores", t.physical_cores); json_raw(&jb, ",");
    json_int(&jb, "logical_cores", t.logical_cores); json_raw(&jb, ",");
    json_double(&jb, "cpu_percent", t.cpu_percent, 1); json_raw(&jb, ",");
    json_double(&jb, "total_ram_gb", t.total_ram_gb, 1); json_raw(&jb, ",");
    json_double(&jb, "used_ram_gb", t.used_ram_gb, 1); json_raw(&jb, ",");
    json_double(&jb, "ram_percent", t.ram_percent, 1); json_raw(&jb, ",");
    json_int(&jb, "total_processes", t.total_processes); json_raw(&jb, ",");
    json_int(&jb, "total_threads", t.total_threads); json_raw(&jb, ",");
    json_str(&jb, "uptime_formatted", t.uptime_formatted); json_raw(&jb, ",");

    json_str(&jb, "foreground_app_title", t.foreground_app_title); json_raw(&jb, ",");
    json_str(&jb, "foreground_app_exe", t.foreground_app_exe); json_raw(&jb, ",");
    json_int(&jb, "foreground_pid", (long long)t.foreground_pid); json_raw(&jb, ",");

    // Affinity mask options
    json_printf(&jb, "\"affinity\":{\"all\":%llu,\"single\":%llu,\"dual\":%llu},",
                (unsigned long long)t.affinity_mask_all,
                (unsigned long long)t.affinity_mask_contention_single,
                (unsigned long long)t.affinity_mask_contention_dual);

    // CPU history
    json_raw(&jb, "\"history\":[");
    for (int i = 0; i < t.history_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        json_printf(&jb, "{\"t\":\"%s\",\"p\":%.1f}", t.history[i].timestamp, t.history[i].cpu_percent);
    }
    json_raw(&jb, "]}");

    send_response(sock, 200, "application/json", jb.data, jb.length);
    json_free(&jb);
}

static void append_run_result_json(JsonBuilder* jb, const AlgorithmRunResult* res, const char* key) {
    json_printf(jb, "\"%s\":{", key);
    json_str(jb, "name", res->algorithm_name); json_raw(jb, ",");
    json_str(jb, "code", res->algorithm_code); json_raw(jb, ",");
    json_double(jb, "avg_interactive_response_time_ms", res->avg_interactive_response_time_ms, 2); json_raw(jb, ",");
    json_double(jb, "p95_interactive_response_time_ms", res->p95_interactive_response_time_ms, 2); json_raw(jb, ",");
    json_double(jb, "avg_interactive_waiting_time_ms", res->avg_interactive_waiting_time_ms, 2); json_raw(jb, ",");
    json_double(jb, "avg_background_turnaround_time_ms", res->avg_background_turnaround_time_ms, 2); json_raw(jb, ",");
    json_double(jb, "background_makespan_ms", res->background_makespan_ms, 2); json_raw(jb, ",");
    json_double(jb, "total_benchmark_duration_ms", res->total_benchmark_duration_ms, 2); json_raw(jb, ",");
    json_int(jb, "total_preemptions", res->total_preemptions); json_raw(jb, ",");
    json_int(jb, "total_context_switches", res->total_context_switches); json_raw(jb, ",");

    // Per-Worker Detailed Metrics (Arrival, First Dispatch, CPU Service, Waiting, Turnaround)
    json_raw(jb, "\"workers\":[");
    for (int i = 0; i < res->task_count; i++) {
        if (i > 0) json_raw(jb, ",");
        const ManagedWorker* w = &res->workers[i];
        char esc[128];
        json_escape(w->def.name, esc, sizeof(esc));
        json_printf(jb, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"pri\":%d,\"arr\":%.2f,\"disp\":%.2f,\"comp\":%.2f,\"cpu_ms\":%.2f,\"wait_ms\":%.2f,\"tat_ms\":%.2f,\"resp_ms\":%.2f,\"preempts\":%d}",
                    w->def.id, esc, (int)w->def.type, w->effective_priority,
                    w->actual_arrival_ms, w->first_dispatch_ms, w->completion_ms,
                    w->total_cpu_time_ms, w->waiting_time_ms, w->turnaround_time_ms,
                    w->response_time_ms, w->times_preempted);
    }
    json_raw(jb, "],");

    // Slices
    json_raw(jb, "\"slices\":[");
    for (int i = 0; i < res->slice_count; i++) {
        if (i > 0) json_raw(jb, ",");
        char esc[128];
        json_escape(res->slices[i].task_name, esc, sizeof(esc));
        json_printf(jb, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"start\":%.2f,\"end\":%.2f,\"dur\":%.2f,\"preempted\":%d}",
                    res->slices[i].task_id, esc, (int)res->slices[i].type,
                    res->slices[i].start_ms, res->slices[i].end_ms,
                    res->slices[i].duration_ms, res->slices[i].was_preempted);
    }
    json_raw(jb, "],");

    // Transitions
    json_raw(jb, "\"transitions\":[");
    int limit = res->transition_count < 80 ? res->transition_count : 80;
    for (int i = 0; i < limit; i++) {
        if (i > 0) json_raw(jb, ",");
        char esc_n[128], esc_r[128];
        json_escape(res->transitions[i].task_name, esc_n, sizeof(esc_n));
        json_escape(res->transitions[i].trigger_reason, esc_r, sizeof(esc_r));
        json_printf(jb, "{\"t\":%.2f,\"id\":%d,\"name\":\"%s\",\"from\":%d,\"to\":%d,\"reason\":\"%s\"}",
                    res->transitions[i].timestamp_ms, res->transitions[i].task_id,
                    esc_n, (int)res->transitions[i].from_state, (int)res->transitions[i].to_state,
                    esc_r);
    }
    json_raw(jb, "]}");
}

static void handle_api_experiment_status(SOCKET sock) {
    ExperimentReport rep;
    experiment_get_status(&rep);

    JsonBuilder jb;
    json_init(&jb, 32768);
    json_raw(&jb, "{");

    json_int(&jb, "status_code", (int)rep.status); json_raw(&jb, ",");
    const char* st_names[] = { "IDLE", "CALIBRATING", "RUNNING_FCFS", "FCFS_DONE", "RUNNING_ADAPTIVE", "COMPLETED", "CANCELLED", "ERROR" };
    json_str(&jb, "status_text", st_names[rep.status % 8]); json_raw(&jb, ",");
    json_str(&jb, "step_description", rep.current_step_description); json_raw(&jb, ",");
    json_double(&jb, "progress_percent", rep.progress_percent, 1); json_raw(&jb, ",");
    json_double(&jb, "elapsed_seconds", rep.elapsed_seconds, 1); json_raw(&jb, ",");

    json_str(&jb, "cpu_model", rep.cpu_model); json_raw(&jb, ",");
    json_int(&jb, "logical_cores", rep.logical_cores); json_raw(&jb, ",");
    json_int(&jb, "affinity_core_count", rep.affinity_core_count); json_raw(&jb, ",");

    // Tasks definition summary
    json_raw(&jb, "\"tasks\":[");
    for (int i = 0; i < rep.workload.task_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        char esc_n[128], esc_d[128];
        json_escape(rep.workload.tasks[i].name, esc_n, sizeof(esc_n));
        json_escape(rep.workload.tasks[i].description, esc_d, sizeof(esc_d));
        json_printf(&jb, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"priority\":%d,\"target_ms\":%.1f,\"arrival_offset\":%.1f,\"desc\":\"%s\"}",
                    rep.workload.tasks[i].id, esc_n, (int)rep.workload.tasks[i].type,
                    rep.workload.tasks[i].base_priority, rep.workload.tasks[i].target_duration_ms,
                    rep.workload.tasks[i].arrival_offset_ms, esc_d);
    }
    json_raw(&jb, "],");

    // Results (if completed or in progress)
    append_run_result_json(&jb, &rep.fcfs_result, "fcfs");
    json_raw(&jb, ",");
    append_run_result_json(&jb, &rep.adaptive_result, "adaptive");
    json_raw(&jb, ",");

    // Adaptive policy context
    char esc_reason[512];
    json_escape(rep.adaptive_ctx.adaptive_reasoning, esc_reason, sizeof(esc_reason));
    json_printf(&jb, "\"adaptive_ctx\":{\"host_cpu\":%.1f,\"aging_thresh\":%d,\"checkpoint_ms\":%.1f,\"reason\":\"%s\"},",
                rep.adaptive_ctx.host_cpu_load_percent, rep.adaptive_ctx.aging_threshold_ms,
                rep.adaptive_ctx.checkpoint_interval_ms, esc_reason);

    // Comparison summary
    json_double(&jb, "response_time_improvement_pct", rep.response_time_improvement_pct, 1); json_raw(&jb, ",");
    json_double(&jb, "waiting_time_improvement_pct", rep.waiting_time_improvement_pct, 1); json_raw(&jb, ",");
    json_double(&jb, "background_makespan_delta_pct", rep.background_makespan_delta_pct, 1); json_raw(&jb, ",");
    json_int(&jb, "preemption_count", rep.preemption_count); json_raw(&jb, ",");
    json_int(&jb, "context_switch_delta", rep.context_switch_delta); json_raw(&jb, ",");

    json_str(&jb, "empirical_conclusion", rep.empirical_conclusion); json_raw(&jb, ",");
    json_str(&jb, "trade_off_analysis", rep.trade_off_analysis);

    json_raw(&jb, "}");

    send_response(sock, 200, "application/json", jb.data, jb.length);
    json_free(&jb);
}

static void handle_api_telemetry_capture(SOCKET sock, const char* body) {
    int duration = 5;
    if (body) {
        const char* p = strstr(body, "\"duration_seconds\":");
        if (p) sscanf(p + 19, "%d", &duration);
    }
    if (duration < 1) duration = 5;
    if (duration > 15) duration = 15;

    if (telemetry_start_capture(duration)) {
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"status\":\"started\",\"duration_seconds\":%d}", duration);
        send_response(sock, 200, "application/json", resp, strlen(resp));
    } else {
        const char* resp = "{\"status\":\"busy\",\"message\":\"Capture already in progress.\"}";
        send_response(sock, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_telemetry_capture_status(SOCKET sock) {
    SystemCaptureResult cap;
    telemetry_get_capture_status(&cap);

    JsonBuilder jb;
    json_init(&jb, 4096);
    json_raw(&jb, "{");

    json_bool(&jb, "is_capturing", cap.is_capturing); json_raw(&jb, ",");
    json_bool(&jb, "is_complete", cap.is_complete); json_raw(&jb, ",");
    json_double(&jb, "progress_percent", cap.progress_percent, 1); json_raw(&jb, ",");
    json_int(&jb, "duration_seconds", cap.duration_seconds); json_raw(&jb, ",");
    json_double(&jb, "elapsed_seconds", cap.elapsed_seconds, 1); json_raw(&jb, ",");
    json_int(&jb, "total_processes", cap.total_processes_observed); json_raw(&jb, ",");
    json_double(&jb, "avg_cpu_percent", cap.avg_cpu_percent, 1); json_raw(&jb, ",");
    json_int(&jb, "aging_threshold_ms", cap.recommended_aging_threshold_ms); json_raw(&jb, ",");
    json_str(&jb, "summary", cap.host_activity_summary); json_raw(&jb, ",");

    json_raw(&jb, "\"top_processes\":[");
    for (int i = 0; i < cap.top_process_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        char esc_p[128];
        json_escape(cap.top_processes[i].name, esc_p, sizeof(esc_p));
        json_printf(&jb, "{\"pid\":%u,\"name\":\"%s\",\"cpu_percent\":%.1f}",
                    (unsigned int)cap.top_processes[i].pid, esc_p, cap.top_processes[i].cpu_percent);
    }
    json_raw(&jb, "],");

    json_raw(&jb, "\"browser_workload\":{");
    json_bool(&jb, "detected", cap.browser_process_count > 0); json_raw(&jb, ",");
    json_str(&jb, "name", cap.detected_browser_name); json_raw(&jb, ",");
    json_int(&jb, "process_count", cap.browser_process_count); json_raw(&jb, ",");
    json_double(&jb, "total_cpu_percent", cap.browser_total_cpu, 1); json_raw(&jb, ",");
    json_raw(&jb, "\"top_processes\":[");
    for (int i = 0; i < cap.top_browser_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        char esc_b[128];
        json_escape(cap.browser_processes[i].name, esc_b, sizeof(esc_b));
        json_printf(&jb, "{\"pid\":%u,\"name\":\"%s\",\"cpu_percent\":%.1f}",
                    (unsigned int)cap.browser_processes[i].pid, esc_b, cap.browser_processes[i].cpu_percent);
    }
    json_raw(&jb, "]}");

    json_raw(&jb, "}");

    send_response(sock, 200, "application/json", jb.data, jb.length);
    json_free(&jb);
}

static void handle_api_workload_prepare(SOCKET sock, const char* body) {
    char mode[32] = "standard";
    char intensity[32] = "balanced";

    if (body) {
        const char* p_mode = strstr(body, "\"mode\":");
        if (p_mode) {
            sscanf(p_mode + 7, " \"%31[^\"]\"", mode);
        }
        const char* p_int = strstr(body, "\"intensity\":");
        if (p_int) {
            sscanf(p_int + 12, " \"%31[^\"]\"", intensity);
        }
    }

    experiment_prepare_workload(mode, intensity);

    ExperimentReport rep;
    experiment_get_status(&rep);

    JsonBuilder jb;
    json_init(&jb, 4096);
    json_raw(&jb, "{");
    json_str(&jb, "status", "prepared"); json_raw(&jb, ",");
    json_str(&jb, "profile_name", rep.workload.profile_name); json_raw(&jb, ",");
    json_int(&jb, "task_count", rep.workload.task_count); json_raw(&jb, ",");
    json_double(&jb, "total_work_ms", rep.workload.total_work_ms, 1); json_raw(&jb, ",");

    json_raw(&jb, "\"tasks\":[");
    for (int i = 0; i < rep.workload.task_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        char esc_n[128], esc_d[128];
        json_escape(rep.workload.tasks[i].name, esc_n, sizeof(esc_n));
        json_escape(rep.workload.tasks[i].description, esc_d, sizeof(esc_d));
        json_printf(&jb, "{\"id\":%d,\"name\":\"%s\",\"type\":%d,\"priority\":%d,\"target_ms\":%.1f,\"arrival_offset\":%.1f,\"desc\":\"%s\"}",
                    rep.workload.tasks[i].id, esc_n, (int)rep.workload.tasks[i].type,
                    rep.workload.tasks[i].base_priority, rep.workload.tasks[i].target_duration_ms,
                    rep.workload.tasks[i].arrival_offset_ms, esc_d);
    }
    json_raw(&jb, "]}");

    send_response(sock, 200, "application/json", jb.data, jb.length);
    json_free(&jb);
}

static void handle_api_experiment_run_fcfs(SOCKET sock) {
    if (experiment_run_fcfs(0x1)) {
        const char* resp = "{\"status\":\"started\",\"mode\":\"fcfs\",\"message\":\"FCFS benchmark executing on Core 0.\"}";
        send_response(sock, 200, "application/json", resp, strlen(resp));
    } else {
        const char* resp = "{\"status\":\"busy\",\"message\":\"An experiment is already in progress.\"}";
        send_response(sock, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_experiment_inject_task(SOCKET sock, const char* body) {
    char name[64] = "I_Urgent_Request";
    if (body) {
        const char* p = strstr(body, "\"name\":");
        if (p) sscanf(p + 7, " \"%63[^\"]\"", name);
    }

    if (experiment_inject_urgent_task(name)) {
        const char* resp = "{\"status\":\"injected\",\"message\":\"Urgent request injected into active ready queue.\"}";
        send_response(sock, 200, "application/json", resp, strlen(resp));
    } else {
        const char* resp = "{\"status\":\"rejected\",\"message\":\"Cannot inject task: FCFS benchmark is not actively running.\"}";
        send_response(sock, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_experiment_run_adaptive(SOCKET sock) {
    if (experiment_run_adaptive(0x1)) {
        const char* resp = "{\"status\":\"started\",\"mode\":\"adaptive\",\"message\":\"Adaptive benchmark executing on identical workload.\"}";
        send_response(sock, 200, "application/json", resp, strlen(resp));
    } else {
        const char* resp = "{\"status\":\"busy\",\"message\":\"An experiment is already in progress.\"}";
        send_response(sock, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_experiment_run(SOCKET sock, const char* body) {
    DWORD_PTR mask = 0;
    double scale = 1.0;
    BOOL use_contention = TRUE;

    if (body) {
        const char* p_scale = strstr(body, "\"scale\":");
        if (p_scale) sscanf(p_scale + 8, "%lf", &scale);

        const char* p_aff = strstr(body, "\"affinity_cores\":");
        if (p_aff) {
            int cores = 1;
            sscanf(p_aff + 17, "%d", &cores);
            if (cores == 2) {
                mask = 0x3; // Cores 0-1
                use_contention = FALSE;
            } else if (cores >= 4) {
                mask = 0xF; // Cores 0-3
                use_contention = FALSE;
            } else {
                mask = 0x1; // Core 0 default
                use_contention = TRUE;
            }
        }
    }

    if (experiment_start(mask, scale, use_contention)) {
        const char* resp = "{\"status\":\"started\",\"message\":\"Live scheduling experiment launched.\"}";
        send_response(sock, 200, "application/json", resp, strlen(resp));
    } else {
        const char* resp = "{\"status\":\"busy\",\"message\":\"An experiment is already in progress.\"}";
        send_response(sock, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_experiment_cancel(SOCKET sock) {
    experiment_cancel();
    const char* resp = "{\"status\":\"cancelling\"}";
    send_response(sock, 200, "application/json", resp, strlen(resp));
}

static void handle_api_real_browser_detect(SOCKET sock) {
    DetectedBrowser chrome, edge;
    real_browser_detect_browsers(&chrome, &edge);

    JsonBuilder jb;
    json_init(&jb, 1024);
    json_raw(&jb, "{");
    json_raw(&jb, "\"chrome\":{");
    json_bool(&jb, "available", chrome.is_available); json_raw(&jb, ",");
    json_str(&jb, "name", chrome.name); json_raw(&jb, ",");
    json_str(&jb, "path", chrome.exe_path);
    json_raw(&jb, "},");
    json_raw(&jb, "\"edge\":{");
    json_bool(&jb, "available", edge.is_available); json_raw(&jb, ",");
    json_str(&jb, "name", edge.name); json_raw(&jb, ",");
    json_str(&jb, "path", edge.exe_path);
    json_raw(&jb, "}}");

    send_response(sock, 200, "application/json", jb.data, jb.length);
    json_free(&jb);
}

static void handle_api_real_browser_start(SOCKET sock, const char* body) {
    char policy[32] = "fcfs";
    char workload[32] = "offline";
    char browser[32] = "chrome";
    int tabs = 10;

    if (body) {
        const char* p_pol = strstr(body, "\"policy\":");
        if (p_pol) sscanf(p_pol + 9, " \"%31[^\"]\"", policy);

        const char* p_work = strstr(body, "\"workload\":");
        if (p_work) sscanf(p_work + 11, " \"%31[^\"]\"", workload);

        const char* p_br = strstr(body, "\"browser\":");
        if (p_br) sscanf(p_br + 10, " \"%31[^\"]\"", browser);

        const char* p_tabs = strstr(body, "\"tabs\":");
        if (p_tabs) sscanf(p_tabs + 7, "%d", &tabs);
    }

    if (real_browser_start_benchmark(policy, workload, browser, tabs)) {
        char resp[256];
        snprintf(resp, sizeof(resp), "{\"status\":\"started\",\"policy\":\"%s\",\"tabs\":%d}", policy, tabs);
        send_response(sock, 200, "application/json", resp, strlen(resp));
    } else {
        const char* resp = "{\"status\":\"busy\",\"message\":\"A browser benchmark is already running.\"}";
        send_response(sock, 400, "application/json", resp, strlen(resp));
    }
}

static void handle_api_real_browser_report_tab(SOCKET sock, const char* body) {
    int tab_id = 1;
    char policy[32] = "fcfs";
    double nav_load_ms = 0.0;
    double dom_render_ms = 0.0;
    double event_latency_ms = 0.0;
    char url[256] = "";

    if (body) {
        const char* p_id = strstr(body, "\"tab_id\":");
        if (p_id) sscanf(p_id + 9, "%d", &tab_id);

        const char* p_pol = strstr(body, "\"policy\":");
        if (p_pol) sscanf(p_pol + 9, " \"%31[^\"]\"", policy);

        const char* p_load = strstr(body, "\"nav_load_ms\":");
        if (p_load) sscanf(p_load + 14, "%lf", &nav_load_ms);

        const char* p_dom = strstr(body, "\"dom_render_ms\":");
        if (p_dom) sscanf(p_dom + 16, "%lf", &dom_render_ms);

        const char* p_evt = strstr(body, "\"event_latency_ms\":");
        if (p_evt) sscanf(p_evt + 19, "%lf", &event_latency_ms);

        const char* p_url = strstr(body, "\"url\":");
        if (p_url) sscanf(p_url + 6, " \"%255[^\"]\"", url);
    }

    real_browser_record_tab_metric(tab_id, policy, nav_load_ms, dom_render_ms, event_latency_ms, url);
    const char* resp = "{\"status\":\"recorded\"}";
    send_response(sock, 200, "application/json", resp, strlen(resp));
}

static void handle_api_real_browser_status(SOCKET sock) {
    RealBrowserRunResult cur;
    RealBrowserComparisonReport rep;
    real_browser_get_status(&cur, &rep);

    JsonBuilder jb;
    json_init(&jb, 8192);
    json_raw(&jb, "{");

    // Current run state
    json_int(&jb, "state", (int)cur.state); json_raw(&jb, ",");
    json_str(&jb, "policy", cur.current_policy); json_raw(&jb, ",");
    json_str(&jb, "workload", cur.workload_type); json_raw(&jb, ",");
    json_int(&jb, "target_tabs", cur.target_tab_count); json_raw(&jb, ",");
    json_int(&jb, "reported_tabs", cur.reported_tab_count); json_raw(&jb, ",");
    json_double(&jb, "avg_nav_load_ms", cur.avg_nav_load_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "avg_event_latency_ms", cur.avg_event_latency_ms, 1); json_raw(&jb, ",");
    json_bool(&jb, "bg_work_active", cur.bg_work_active); json_raw(&jb, ",");
    json_int(&jb, "bg_thread_count", cur.bg_thread_count); json_raw(&jb, ",");
    json_str(&jb, "bg_priority_label", cur.bg_priority_label); json_raw(&jb, ",");
    json_double(&jb, "bg_work_duration_ms", cur.bg_work_duration_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "elapsed_ms", cur.bench_elapsed_ms, 1); json_raw(&jb, ",");
    json_str(&jb, "status_message", cur.status_message); json_raw(&jb, ",");

    // Tab metrics list
    json_raw(&jb, "\"tabs\":[");
    for (int i = 0; i < cur.reported_tab_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        json_printf(&jb, "{\"id\":%d,\"load_ms\":%.1f,\"dom_ms\":%.1f,\"event_ms\":%.1f}",
                    cur.tab_metrics[i].tab_id, cur.tab_metrics[i].nav_load_ms,
                    cur.tab_metrics[i].dom_render_ms, cur.tab_metrics[i].event_latency_ms);
    }
    json_raw(&jb, "],");

    // Transitions
    json_raw(&jb, "\"transitions\":[");
    for (int i = 0; i < cur.transition_count; i++) {
        if (i > 0) json_raw(&jb, ",");
        char esc[128];
        json_escape(cur.transitions[i].trigger, esc, sizeof(esc));
        json_printf(&jb, "{\"t\":%.1f,\"old_pri\":%d,\"new_pri\":%d,\"trigger\":\"%s\"}",
                    cur.transitions[i].t_ms, cur.transitions[i].old_pri, cur.transitions[i].new_pri, esc);
    }
    json_raw(&jb, "],");

    // Comparison report
    json_raw(&jb, "\"report\":{");
    json_bool(&jb, "has_fcfs", rep.has_fcfs); json_raw(&jb, ",");
    json_bool(&jb, "has_adaptive", rep.has_adaptive); json_raw(&jb, ",");
    json_double(&jb, "fcfs_avg_load_ms", rep.fcfs_result.avg_nav_load_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "adaptive_avg_load_ms", rep.adaptive_result.avg_nav_load_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "fcfs_avg_event_ms", rep.fcfs_result.avg_event_latency_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "adaptive_avg_event_ms", rep.adaptive_result.avg_event_latency_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "fcfs_bg_duration_ms", rep.fcfs_result.bg_work_duration_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "adaptive_bg_duration_ms", rep.adaptive_result.bg_work_duration_ms, 1); json_raw(&jb, ",");
    json_double(&jb, "load_improvement_pct", rep.load_improvement_pct, 1); json_raw(&jb, ",");
    json_double(&jb, "latency_improvement_pct", rep.latency_improvement_pct, 1); json_raw(&jb, ",");
    json_double(&jb, "bg_overhead_pct", rep.bg_overhead_pct, 1); json_raw(&jb, ",");
    json_str(&jb, "empirical_conclusion", rep.empirical_conclusion);
    json_raw(&jb, "}}");

    send_response(sock, 200, "application/json", jb.data, jb.length);
    json_free(&jb);
}

static void handle_api_real_browser_cancel(SOCKET sock) {
    real_browser_cancel();
    const char* resp = "{\"status\":\"cancelled\"}";
    send_response(sock, 200, "application/json", resp, strlen(resp));
}

static DWORD WINAPI client_handler_thread(LPVOID lpParam) {
    SOCKET client = (SOCKET)lpParam;
    char buf[4096];
    int total = 0;

    DWORD to = 2000;
    setsockopt(client, SOL_SOCKET, SO_RCVTIMEO, (const char*)&to, sizeof(to));

    // Read header until \r\n\r\n
    while (total < (int)sizeof(buf) - 1) {
        int n = recv(client, buf + total, (int)sizeof(buf) - 1 - total, 0);
        if (n <= 0) break;
        total += n;
        buf[total] = '\0';
        if (strstr(buf, "\r\n\r\n")) break;
    }

    if (total <= 0) {
        closesocket(client);
        return 0;
    }

    // Check for Content-Length and receive remaining body if any
    char* body_marker = strstr(buf, "\r\n\r\n");
    if (body_marker) {
        int header_len = (int)(body_marker - buf) + 4;
        int body_in_buf = total - header_len;
        int content_len = 0;
        char* cl = strstr(buf, "Content-Length:");
        if (!cl) cl = strstr(buf, "content-length:");
        if (cl) {
            content_len = atoi(cl + 15);
        }
        while (body_in_buf < content_len && total < (int)sizeof(buf) - 1) {
            int n = recv(client, buf + total, (int)sizeof(buf) - 1 - total, 0);
            if (n <= 0) break;
            total += n;
            body_in_buf += n;
            buf[total] = '\0';
        }
    }

    char method[16] = "";
    char path[256] = "";
    sscanf(buf, "%15s %255s", method, path);

    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client, 200, "text/plain", "", 0);
        shutdown(client, SD_SEND);
        closesocket(client);
        return 0;
    }

    char* body = strstr(buf, "\r\n\r\n");
    if (body) body += 4;

    if (strcmp(path, "/") == 0 || strcmp(path, "/index.html") == 0) {
        serve_file(client, "index.html", "text/html; charset=utf-8");
    } else if (strcmp(path, "/app.css") == 0) {
        serve_file(client, "app.css", "text/css");
    } else if (strcmp(path, "/app.js") == 0) {
        serve_file(client, "app.js", "application/javascript");
    } else if (strncmp(path, "/benchmark_tab.html", 19) == 0) {
        serve_file(client, "benchmark_tab.html", "text/html; charset=utf-8");
    } else if (strcmp(path, "/api/telemetry") == 0) {
        handle_api_telemetry(client);
    } else if (strcmp(path, "/api/telemetry/capture") == 0) {
        handle_api_telemetry_capture(client, body);
    } else if (strcmp(path, "/api/telemetry/capture_status") == 0) {
        handle_api_telemetry_capture_status(client);
    } else if (strcmp(path, "/api/workload/prepare") == 0) {
        handle_api_workload_prepare(client, body);
    } else if (strcmp(path, "/api/experiment/status") == 0) {
        handle_api_experiment_status(client);
    } else if (strcmp(path, "/api/experiment/run_fcfs") == 0) {
        handle_api_experiment_run_fcfs(client);
    } else if (strcmp(path, "/api/experiment/inject_task") == 0) {
        handle_api_experiment_inject_task(client, body);
    } else if (strcmp(path, "/api/experiment/run_adaptive") == 0) {
        handle_api_experiment_run_adaptive(client);
    } else if (strcmp(path, "/api/experiment/run") == 0) {
        handle_api_experiment_run(client, body);
    } else if (strcmp(path, "/api/experiment/cancel") == 0) {
        handle_api_experiment_cancel(client);
    } else if (strcmp(path, "/api/real_browser/detect") == 0) {
        handle_api_real_browser_detect(client);
    } else if (strcmp(path, "/api/real_browser/start") == 0) {
        handle_api_real_browser_start(client, body);
    } else if (strcmp(path, "/api/real_browser/report_tab") == 0) {
        handle_api_real_browser_report_tab(client, body);
    } else if (strcmp(path, "/api/real_browser/status") == 0) {
        handle_api_real_browser_status(client);
    } else if (strcmp(path, "/api/real_browser/cancel") == 0) {
        handle_api_real_browser_cancel(client);
    } else {
        const char* nf = "404 Not Found";
        send_response(client, 404, "text/plain", nf, strlen(nf));
    }

    // Graceful TCP shutdown to ensure all response bytes are acknowledged without RST
    shutdown(client, SD_SEND);
    char drain[256];
    while (recv(client, drain, sizeof(drain), 0) > 0) {}
    closesocket(client);
    return 0;
}

int http_server_init(int preferred_port, const char* web_root_override) {
    resolve_web_root(web_root_override);

    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        fprintf(stderr, "[Error] WSAStartup failed.\n");
        return 0;
    }

    // Dynamic port probing: Try preferred_port, preferred_port+1, ...
    int port = preferred_port > 0 ? preferred_port : 8000;
    for (int attempt = 0; attempt < 25; attempt++) {
        s_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s_listen_sock == INVALID_SOCKET) {
            WSACleanup();
            return 0;
        }

        struct sockaddr_in sin;
        memset(&sin, 0, sizeof(sin));
        sin.sin_family = AF_INET;
        sin.sin_port = htons((u_short)(port + attempt));
        sin.sin_addr.s_addr = inet_addr("127.0.0.1"); // Strictly bind localhost

        if (bind(s_listen_sock, (struct sockaddr*)&sin, sizeof(sin)) == 0) {
            if (listen(s_listen_sock, 16) == 0) {
                s_bound_port = port + attempt;
                break;
            }
        }
        closesocket(s_listen_sock);
        s_listen_sock = INVALID_SOCKET;
    }

    if (s_listen_sock == INVALID_SOCKET) {
        fprintf(stderr, "[Error] Could not bind to an available local port in range %d-%d.\n", port, port + 24);
        WSACleanup();
        return 0;
    }

    s_running = TRUE;
    printf("[HTTP] Server bound on http://127.0.0.1:%d\n", s_bound_port);
    printf("[HTTP] Serving web UI from: %s\n", s_web_dir);
    return s_bound_port;
}

void http_server_run(void) {
    if (!s_running || s_listen_sock == INVALID_SOCKET) return;

    printf("[HTTP] Server accept loop running...\n");
    while (s_running) {
        SOCKET client = accept(s_listen_sock, NULL, NULL);
        if (client != INVALID_SOCKET) {
            CreateThread(NULL, 0, client_handler_thread, (LPVOID)client, 0, NULL);
        }
    }

    if (s_listen_sock != INVALID_SOCKET) {
        closesocket(s_listen_sock);
        s_listen_sock = INVALID_SOCKET;
    }
    WSACleanup();
    printf("[HTTP] Server stopped.\n");
}

void http_server_stop(void) {
    s_running = FALSE;
    if (s_listen_sock != INVALID_SOCKET) {
        closesocket(s_listen_sock);
        s_listen_sock = INVALID_SOCKET;
    }
}
