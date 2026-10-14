#include <winsock2.h>
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "system_telemetry.h"
#include "calibration.h"
#include "experiment_runner.h"
#include "http_server.h"
#include "real_browser_benchmark.h"

static HANDLE s_hJob = NULL;

static BOOL WINAPI ConsoleCtrlHandler(DWORD fdwCtrlType) {
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
            printf("\n[System] Shutdown signal received. Stopping experiment and server...\n");
            real_browser_cancel();
            experiment_cancel();
            http_server_stop();
            return TRUE;
        default:
            return FALSE;
    }
}

static void print_banner(void) {
    printf("================================================================================\n");
    printf("       ADAPTIVE CPU SCHEDULING OPTIMIZER - ACADEMIC RESEARCH DEMONSTRATOR       \n");
    printf("      Demonstrating Workload-Aware Priority Scheduling vs FCFS on Windows       \n");
    printf("================================================================================\n\n");
}

static void print_usage(const char* prog) {
    printf("Usage: %s [options]\n\n", prog);
    printf("Options:\n");
    printf("  --port <number>      Preferred HTTP port (default: 8000; probes up to +24)\n");
    printf("  --no-browser         Do not automatically open the default web browser\n");
    printf("  --web-root <path>    Override path to frontend static files\n");
    printf("  --help, -h           Show this help message and exit\n\n");
}

int main(int argc, char* argv[]) {
    int preferred_port = 8000;
    BOOL auto_open_browser = TRUE;
    const char* web_root_override = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            preferred_port = atoi(argv[++i]);
        } else if (strcmp(argv[i], "--no-browser") == 0) {
            auto_open_browser = FALSE;
        } else if (strcmp(argv[i], "--web-root") == 0 && i + 1 < argc) {
            web_root_override = argv[++i];
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    print_banner();

    // Setup Job Object for leak-proof process and worker lifecycle
    s_hJob = CreateJobObject(NULL, NULL);
    if (s_hJob) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION jeli;
        memset(&jeli, 0, sizeof(jeli));
        jeli.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(s_hJob, JobObjectExtendedLimitInformation, &jeli, sizeof(jeli));
        AssignProcessToJobObject(s_hJob, GetCurrentProcess());
    }

    // Set console signal handler for clean graceful shutdown
    SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

    // Initialize telemetry
    telemetry_init();
    real_browser_bench_init();

    SystemTelemetry t;
    telemetry_get(&t);
    printf("[Telemetry] Host Machine:     %s (%s)\n", t.hostname, t.os_name);
    printf("[Telemetry] CPU Model:        %s\n", t.cpu_model);
    printf("[Telemetry] Hardware Cores:   %d Physical, %d Logical Processors\n", t.physical_cores, t.logical_cores);
    printf("[Telemetry] Total Memory:     %.1f GB (%.1f%% currently in use)\n", t.total_ram_gb, t.ram_percent);
    printf("[Telemetry] Running Tasks:    %d processes, %d system threads\n", t.total_processes, t.total_threads);
    if (*t.foreground_app_title) {
        printf("[Telemetry] Active Window:    \"%s\" (%s, PID: %u)\n", 
               t.foreground_app_title, t.foreground_app_exe, (unsigned int)t.foreground_pid);
    }
    printf("\n");

    // Perform real CPU calibration loop
    printf("[Calibration] Running CPU benchmark calibration loop...\n");
    calibration_init();
    calibration_run();
    const MachineCalibration* calib = calibration_get();
    printf("[Calibration] Baseline established: %llu iterations/ms (QPC timer resolution: %.0f Hz)\n\n",
           (unsigned long long)calib->iterations_per_ms, calib->qpc_frequency);

    // Start HTTP server
    int bound_port = http_server_init(preferred_port, web_root_override);
    if (bound_port <= 0) {
        fprintf(stderr, "[Fatal] Failed to bind HTTP server to any local port in range %d-%d.\n", preferred_port, preferred_port + 24);
        if (s_hJob) CloseHandle(s_hJob);
        return 1;
    }

    char url[128];
    snprintf(url, sizeof(url), "http://127.0.0.1:%d", bound_port);
    printf("[Ready] Application interface is accessible at: %s\n", url);
    printf("[Ready] Press Ctrl+C in this console window to shut down the server.\n\n");

    if (auto_open_browser) {
        printf("[Browser] Launching default web browser...\n");
        ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
    }

    // Enter server loop
    http_server_run();

    // Clean exit
    if (s_hJob) {
        CloseHandle(s_hJob);
    }
    printf("[System] Clean shutdown completed. Goodbye.\n");
    return 0;
}
