#include "system_telemetry.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <tlhelp32.h>
#include <psapi.h>

static CRITICAL_SECTION s_telem_cs;
static BOOL s_initialized = FALSE;

static FILETIME s_prev_idle;
static FILETIME s_prev_kernel;
static FILETIME s_prev_user;
static double s_last_cpu_percent = 0.0;
static ULONGLONG s_last_tick = 0;

static TelemetrySample s_history[TELEMETRY_HISTORY_LEN];
static int s_history_count = 0;

static ULONGLONG ft_to_ull(const FILETIME* ft) {
    ULARGE_INTEGER uli;
    uli.LowPart = ft->dwLowDateTime;
    uli.HighPart = ft->dwHighDateTime;
    return uli.QuadPart;
}

static void query_cpu_model(char* out, size_t out_sz) {
    out[0] = '\0';
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        DWORD sz = (DWORD)out_sz;
        DWORD type = REG_SZ;
        if (RegQueryValueExA(hKey, "ProcessorNameString", NULL, &type, (LPBYTE)out, &sz) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            char* p = out;
            while (*p == ' ') p++;
            if (p != out) memmove(out, p, strlen(p) + 1);
            size_t len = strlen(out);
            while (len > 0 && out[len - 1] == ' ') out[--len] = '\0';
            return;
        }
        RegCloseKey(hKey);
    }
    const char* env_cpu = getenv("PROCESSOR_IDENTIFIER");
    if (env_cpu && *env_cpu) {
        strncpy(out, env_cpu, out_sz - 1);
        out[out_sz - 1] = '\0';
    } else {
        strncpy(out, "x86_64 Compatible Processor", out_sz - 1);
    }
}

static void query_cores(int* physical, int* logical) {
    SYSTEM_INFO si;
    GetSystemInfo(&si);
    *logical = (int)si.dwNumberOfProcessors;
    if (*logical <= 0) *logical = 1;

    DWORD len = 0;
    *physical = 0;
    GetLogicalProcessorInformation(NULL, &len);
    if (GetLastError() == ERROR_INSUFFICIENT_BUFFER && len > 0) {
        PSYSTEM_LOGICAL_PROCESSOR_INFORMATION buf = (PSYSTEM_LOGICAL_PROCESSOR_INFORMATION)malloc(len);
        if (buf) {
            if (GetLogicalProcessorInformation(buf, &len)) {
                DWORD cnt = len / sizeof(SYSTEM_LOGICAL_PROCESSOR_INFORMATION);
                for (DWORD i = 0; i < cnt; i++) {
                    if (buf[i].Relationship == RelationProcessorCore) {
                        (*physical)++;
                    }
                }
            }
            free(buf);
        }
    }
    if (*physical <= 0) {
        *physical = (*logical >= 2) ? (*logical / 2) : 1;
    }
}

static double compute_cpu_percent(void) {
    FILETIME idle, kernel, user;
    if (!GetSystemTimes(&idle, &kernel, &user)) {
        return s_last_cpu_percent;
    }

    ULONGLONG now = GetTickCount64();
    if (s_last_tick == 0) {
        s_prev_idle = idle;
        s_prev_kernel = kernel;
        s_prev_user = user;
        s_last_tick = now;
        return 0.0;
    }

    if (now - s_last_tick < 200) {
        return s_last_cpu_percent;
    }

    ULONGLONG i_now = ft_to_ull(&idle);
    ULONGLONG k_now = ft_to_ull(&kernel);
    ULONGLONG u_now = ft_to_ull(&user);

    ULONGLONG i_prev = ft_to_ull(&s_prev_idle);
    ULONGLONG k_prev = ft_to_ull(&s_prev_kernel);
    ULONGLONG u_prev = ft_to_ull(&s_prev_user);

    ULONGLONG i_diff = (i_now >= i_prev) ? (i_now - i_prev) : 0;
    ULONGLONG k_diff = (k_now >= k_prev) ? (k_now - k_prev) : 0;
    ULONGLONG u_diff = (u_now >= u_prev) ? (u_now - u_prev) : 0;

    ULONGLONG total = k_diff + u_diff;
    double pct = 0.0;
    if (total > 0 && total >= i_diff) {
        pct = (double)(total - i_diff) * 100.0 / (double)total;
        if (pct < 0.0) pct = 0.0;
        if (pct > 100.0) pct = 100.0;
    }

    s_prev_idle = idle;
    s_prev_kernel = kernel;
    s_prev_user = user;
    s_last_tick = now;
    s_last_cpu_percent = pct;
    return pct;
}

static void query_foreground_app(char* title_out, size_t title_sz, char* exe_out, size_t exe_sz, DWORD* pid_out) {
    title_out[0] = '\0';
    exe_out[0] = '\0';
    *pid_out = 0;

    HWND hwnd = GetForegroundWindow();
    if (!hwnd) {
        strncpy(title_out, "Desktop / None", title_sz - 1);
        strncpy(exe_out, "System", exe_sz - 1);
        return;
    }

    GetWindowTextA(hwnd, title_out, (int)title_sz - 1);
    title_out[title_sz - 1] = '\0';
    if (!title_out[0]) {
        strncpy(title_out, "Active Window", title_sz - 1);
    }

    DWORD pid = 0;
    GetWindowThreadProcessId(hwnd, &pid);
    *pid_out = pid;

    if (pid > 0) {
        HANDLE hProc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        if (hProc) {
            char path[MAX_PATH];
            DWORD path_len = sizeof(path);
            if (QueryFullProcessImageNameA(hProc, 0, path, &path_len)) {
                char* slash = strrchr(path, '\\');
                const char* src = slash ? (slash + 1) : path;
                size_t i = 0;
                while (i + 1 < exe_sz && src[i]) { exe_out[i] = src[i]; i++; }
                exe_out[i] = '\0';
            }
            CloseHandle(hProc);
        }
    }
    if (!exe_out[0]) {
        const char* def = "Unknown Application";
        size_t i = 0;
        while (i + 1 < exe_sz && def[i]) { exe_out[i] = def[i]; i++; }
        exe_out[i] = '\0';
    }
}

void telemetry_init(void) {
    if (!s_initialized) {
        InitializeCriticalSection(&s_telem_cs);
        GetSystemTimes(&s_prev_idle, &s_prev_kernel, &s_prev_user);
        s_last_tick = GetTickCount64();
        s_initialized = TRUE;
    }
}

double telemetry_get_instant_cpu(void) {
    telemetry_init();
    EnterCriticalSection(&s_telem_cs);
    double pct = compute_cpu_percent();
    LeaveCriticalSection(&s_telem_cs);
    return pct;
}

void telemetry_get(SystemTelemetry* out) {
    if (!out) return;
    telemetry_init();
    memset(out, 0, sizeof(SystemTelemetry));

    // Hostname
    DWORD hlen = sizeof(out->hostname);
    if (!GetComputerNameA(out->hostname, &hlen)) {
        strncpy(out->hostname, "Windows-PC", sizeof(out->hostname) - 1);
    }

    strncpy(out->os_name, "Windows 10/11", sizeof(out->os_name) - 1);
    strncpy(out->architecture, "x86_64", sizeof(out->architecture) - 1);

    query_cpu_model(out->cpu_model, sizeof(out->cpu_model));
    query_cores(&out->physical_cores, &out->logical_cores);

    // Contention affinity masks
    DWORD_PTR proc_mask = 0, sys_mask = 0;
    if (GetProcessAffinityMask(GetCurrentProcess(), &proc_mask, &sys_mask) && proc_mask > 0) {
        out->affinity_mask_all = proc_mask;
    } else {
        out->affinity_mask_all = ((DWORD_PTR)1 << (out->logical_cores > 31 ? 31 : out->logical_cores)) - 1;
    }
    out->affinity_mask_contention_single = 0x1; // Core 0
    out->affinity_mask_contention_dual = (out->logical_cores >= 2) ? 0x3 : 0x1; // Cores 0-1

    // Uptime
    ULONGLONG up_ms = GetTickCount64();
    out->uptime_seconds = (double)up_ms / 1000.0;
    long s = (long)out->uptime_seconds;
    long days = s / 86400;
    long hours = (s % 86400) / 3600;
    long mins = (s % 3600) / 60;
    long secs = s % 60;
    if (days > 0) snprintf(out->uptime_formatted, sizeof(out->uptime_formatted), "%ldd %ldh %ldm", days, hours, mins);
    else if (hours > 0) snprintf(out->uptime_formatted, sizeof(out->uptime_formatted), "%ldh %ldm %lds", hours, mins, secs);
    else snprintf(out->uptime_formatted, sizeof(out->uptime_formatted), "%ldm %lds", mins, secs);

    // RAM
    MEMORYSTATUSEX mem;
    mem.dwLength = sizeof(mem);
    if (GlobalMemoryStatusEx(&mem)) {
        out->total_ram_gb = (double)mem.ullTotalPhys / (1024.0 * 1024.0 * 1024.0);
        DWORDLONG used = mem.ullTotalPhys - mem.ullAvailPhys;
        out->used_ram_gb = (double)used / (1024.0 * 1024.0 * 1024.0);
        out->ram_percent = (double)mem.dwMemoryLoad;
    }

    // Processes and threads count
    out->total_processes = 0;
    out->total_threads = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap != INVALID_HANDLE_VALUE) {
        PROCESSENTRY32 pe;
        pe.dwSize = sizeof(pe);
        if (Process32First(snap, &pe)) {
            do {
                out->total_processes++;
                out->total_threads += pe.cntThreads;
            } while (Process32Next(snap, &pe));
        }
        CloseHandle(snap);
    }

    // Foreground app
    query_foreground_app(out->foreground_app_title, sizeof(out->foreground_app_title),
                         out->foreground_app_exe, sizeof(out->foreground_app_exe),
                         &out->foreground_pid);

    // CPU % and history
    EnterCriticalSection(&s_telem_cs);
    out->cpu_percent = compute_cpu_percent();

    time_t t = time(NULL);
    struct tm* tm_info = localtime(&t);
    char tbuf[16] = "00:00:00";
    if (tm_info) strftime(tbuf, sizeof(tbuf), "%H:%M:%S", tm_info);

    if (s_history_count < TELEMETRY_HISTORY_LEN) {
        snprintf(s_history[s_history_count].timestamp, sizeof(s_history[s_history_count].timestamp), "%s", tbuf);
        s_history[s_history_count].cpu_percent = out->cpu_percent;
        s_history_count++;
    } else {
        memmove(&s_history[0], &s_history[1], sizeof(TelemetrySample) * (TELEMETRY_HISTORY_LEN - 1));
        snprintf(s_history[TELEMETRY_HISTORY_LEN - 1].timestamp, sizeof(s_history[TELEMETRY_HISTORY_LEN - 1].timestamp), "%s", tbuf);
        s_history[TELEMETRY_HISTORY_LEN - 1].cpu_percent = out->cpu_percent;
    }
    out->history_count = s_history_count;
    memcpy(out->history, s_history, sizeof(TelemetrySample) * s_history_count);
    LeaveCriticalSection(&s_telem_cs);
}
