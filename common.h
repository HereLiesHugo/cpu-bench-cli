#ifndef COMMON_H
#define COMMON_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/* Platform detection */
#if defined(_WIN32) || defined(_WIN64)
    #define PLATFORM_WINDOWS 1
    #define PLATFORM_LINUX 0
    #define PLATFORM_MACOS 0
#elif defined(__APPLE__) && defined(__MACH__)
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_LINUX 0
    #define PLATFORM_MACOS 1
#elif defined(__linux__)
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_LINUX 1
    #define PLATFORM_MACOS 0
#else
    #define PLATFORM_WINDOWS 0
    #define PLATFORM_LINUX 0
    #define PLATFORM_MACOS 0
#endif

/* Benchmark configuration constants */
#define SIEVE_LIMIT 10000000
#define SIEVE_EXPECTED_PRIMES 664579
#define MATRIX_SIZE 1024
#define NUM_ITERATIONS 3
#define WARMUP_ITERATIONS 1
#define THERMAL_WARNING_DELTA 15
#define LOAD_WARNING_THRESHOLD 0.5

/* Command-line options structure */
typedef struct {
    bool single_mode;
    bool multi_mode;
    int thread_count;  /* 0 = auto (physical cores) */
    bool json_output;
    bool verbose;
} benchmark_options_t;

/* Timing structure */
typedef struct {
    struct timespec start;
    struct timespec end;
} bench_timer_t;

/* Result structure for a single workload run */
typedef struct {
    double elapsed_ms;      /* Wall-clock time in milliseconds */
    uint64_t iterations;    /* Number of iterations completed */
    bool valid;             /* Whether result validation passed */
} workload_result_t;

/* Complete benchmark results */
typedef struct {
    /* System info */
    char cpu_model[256];
    int logical_cores;
    int physical_cores;
    
    /* Single-core results */
    workload_result_t single_sieve;
    workload_result_t single_matrix;
    
    /* Multi-core results */
    workload_result_t multi_sieve;
    workload_result_t multi_matrix;
    int threads_used;
    
    /* Health checks */
    int temp_delta;
    bool thermal_warning;
    double load_before;
    bool load_warning;
} benchmark_results_t;

/* Utility macros for timing */
#define TIMER_START(t) clock_gettime(CLOCK_MONOTONIC, &(t).start)
#define TIMER_STOP(t) clock_gettime(CLOCK_MONOTONIC, &(t).end)
#define TIMER_MS(t) (((t).end.tv_sec - (t).start.tv_sec) * 1000.0 + \
                     ((t).end.tv_nsec - (t).start.tv_nsec) / 1000000.0)

/* Utility function to get current time in milliseconds */
static inline double get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1000000.0;
}

#endif /* COMMON_H */
