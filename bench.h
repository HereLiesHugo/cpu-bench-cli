#ifndef BENCH_H
#define BENCH_H

#include "common.h"
#include "sysinfo.h"
#include "workloads.h"
#include <pthread.h>

/* Thread data for multi-core benchmark */
typedef struct {
    int thread_id;
    int cpu_id;
    workload_type_t workload;
    bool verbose;
    pthread_barrier_t *barrier;
    thread_workload_result_t result;
} bench_thread_data_t;

/* Single-core benchmark runner */
/* Pins thread to core 0, runs warmup + 3 timed iterations */
workload_result_t run_single_core_benchmark(workload_type_t workload, bool verbose);

/* Multi-core benchmark runner */
/* Spawns N threads, each pinned to distinct core, synchronized via barrier */
workload_result_t run_multi_core_benchmark(workload_type_t workload, int num_threads, bool verbose);

/* Thread worker function for multi-core benchmarks */
void *benchmark_thread_worker(void *arg);

/* Pin current thread to specific CPU */
/* Platform-specific implementation */
bool pin_thread_to_cpu(int cpu_id);

/* Get number of physical cores (not logical) */
int get_physical_core_count(void);

#endif /* BENCH_H */
