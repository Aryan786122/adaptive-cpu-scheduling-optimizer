#include "calibration.h"
#include <stdio.h>
#include <stdlib.h>

static LARGE_INTEGER s_qpc_freq;
static MachineCalibration s_calibration = {0};
static BOOL s_initialized = FALSE;

void calibration_init(void) {
    if (!s_initialized) {
        QueryPerformanceFrequency(&s_qpc_freq);
        s_calibration.qpc_frequency = (double)s_qpc_freq.QuadPart;
        calibration_run();
        s_initialized = TRUE;
    }
}

double qpc_now_ms(void) {
    LARGE_INTEGER now;
    QueryPerformanceCounter(&now);
    if (s_qpc_freq.QuadPart == 0) return 0.0;
    return ((double)now.QuadPart * 1000.0) / (double)s_qpc_freq.QuadPart;
}

double qpc_diff_ms(LARGE_INTEGER start, LARGE_INTEGER end) {
    if (s_qpc_freq.QuadPart == 0) return 0.0;
    return ((double)(end.QuadPart - start.QuadPart) * 1000.0) / (double)s_qpc_freq.QuadPart;
}

void calibration_run(void) {
    if (s_qpc_freq.QuadPart == 0) {
        QueryPerformanceFrequency(&s_qpc_freq);
    }

    // Benchmark a tight loop for ~25 milliseconds to find machine speed
    unsigned long long test_iters = 1000000ULL;
    LARGE_INTEGER t0, t1;
    QueryPerformanceCounter(&t0);

    volatile double acc = 1.0;
    for (unsigned long long i = 0; i < test_iters; i++) {
        acc = acc * 1.0000001 + (double)(i & 0x7);
    }

    QueryPerformanceCounter(&t1);
    double elapsed_ms = qpc_diff_ms(t0, t1);
    if (elapsed_ms < 0.1) elapsed_ms = 0.1;

    // Iterations per millisecond on this core
    s_calibration.iterations_per_ms = (unsigned long long)((double)test_iters / elapsed_ms);
    if (s_calibration.iterations_per_ms < 1000ULL) s_calibration.iterations_per_ms = 1000ULL;

    s_calibration.calibrated_ms_per_unit = 10.0; // 1 work unit = ~10ms target
    s_calibration.calibration_duration_ms = elapsed_ms;
    s_calibration.is_calibrated = TRUE;
}

const MachineCalibration* calibration_get(void) {
    if (!s_initialized) calibration_init();
    return &s_calibration;
}

unsigned long long execute_compute_block(unsigned long long iterations, volatile LONG* yield_request_flag) {
    // Dynamic checkpoint stride calibrated to ~15ms of CPU compute
    unsigned long long iters_per_ms = s_calibration.iterations_per_ms;
    if (iters_per_ms < 1000ULL) iters_per_ms = 1000ULL;
    unsigned long long checkpoint_stride = (unsigned long long)((double)iters_per_ms * 15.0);
    if (checkpoint_stride < 10000ULL) checkpoint_stride = 10000ULL;

    unsigned long long completed = 0;
    volatile double acc = 1.0001;

    while (completed < iterations) {
        unsigned long long batch = iterations - completed;
        if (batch > checkpoint_stride) batch = checkpoint_stride;

        for (unsigned long long i = 0; i < batch; i++) {
            acc = acc * 1.0000001 + (double)((completed + i) & 0xF);
        }
        completed += batch;

        // Check cooperative preemption checkpoint
        if (yield_request_flag && InterlockedCompareExchange(yield_request_flag, 0, 0) != 0) {
            break; // Yield gracefully at checkpoint
        }
    }
    return completed;
}

unsigned long long execute_memory_block(unsigned long long iterations, volatile LONG* yield_request_flag) {
    const size_t NUM_INTS = 16384; // 64 KB cache-sized stride buffer
    static volatile int mem_buf[16384];

    // Dynamic checkpoint stride calibrated to ~15ms of memory stride passes
    unsigned long long iters_per_ms = s_calibration.iterations_per_ms;
    if (iters_per_ms < 1000ULL) iters_per_ms = 1000ULL;
    // Memory strides have cache misses, so each iteration takes ~3x arithmetic time
    unsigned long long checkpoint_stride = (unsigned long long)((double)iters_per_ms * 5.0);
    if (checkpoint_stride < 5000ULL) checkpoint_stride = 5000ULL;

    unsigned long long completed = 0;
    size_t idx = 0;

    while (completed < iterations) {
        unsigned long long batch = iterations - completed;
        if (batch > checkpoint_stride) batch = checkpoint_stride;

        for (unsigned long long i = 0; i < batch; i++) {
            idx = (idx + 1021) & (NUM_INTS - 1);
            mem_buf[idx] += (int)i;
        }
        completed += batch;

        if (yield_request_flag && InterlockedCompareExchange(yield_request_flag, 0, 0) != 0) {
            break; // Yield gracefully at checkpoint
        }
    }
    return completed;
}
