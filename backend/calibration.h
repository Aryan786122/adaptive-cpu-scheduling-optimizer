#pragma once
#include <windows.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    double qpc_frequency;
    unsigned long long iterations_per_ms;
    double calibrated_ms_per_unit;
    double calibration_duration_ms;
    BOOL is_calibrated;
} MachineCalibration;

void calibration_init(void);
const MachineCalibration* calibration_get(void);
void calibration_run(void);
double qpc_now_ms(void);
double qpc_diff_ms(LARGE_INTEGER start, LARGE_INTEGER end);

// Real computation loop executed by worker threads
// Performs authentic mathematical computation in blocks checking an atomic yield flag
unsigned long long execute_compute_block(unsigned long long iterations, volatile LONG* yield_request_flag);
unsigned long long execute_memory_block(unsigned long long iterations, volatile LONG* yield_request_flag);

#ifdef __cplusplus
}
#endif
