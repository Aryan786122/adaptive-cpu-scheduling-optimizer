#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TELEMETRY_HISTORY_LEN 30

typedef struct {
    char timestamp[16];
    double cpu_percent;
} TelemetrySample;

typedef struct {
    char hostname[128];
    char os_name[128];
    char architecture[64];
    char cpu_model[256];
    int physical_cores;
    int logical_cores;
    DWORD_PTR affinity_mask_all;
    DWORD_PTR affinity_mask_contention_single;
    DWORD_PTR affinity_mask_contention_dual;
    double total_ram_gb;
    double used_ram_gb;
    double ram_percent;
    double cpu_percent;
    int total_processes;
    int total_threads;
    double uptime_seconds;
    char uptime_formatted[64];
    char foreground_app_title[256];
    char foreground_app_exe[128];
    DWORD foreground_pid;
    TelemetrySample history[TELEMETRY_HISTORY_LEN];
    int history_count;
} SystemTelemetry;

void telemetry_init(void);
void telemetry_get(SystemTelemetry* out);
double telemetry_get_instant_cpu(void);

#ifdef __cplusplus
}
#endif
