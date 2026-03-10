#ifndef WORKLOADS_H
#define WORKLOADS_H

#include "common.h"
#include <stddef.h>
#include <pthread.h>

/* Cross-platform pthread barrier implementation (macOS doesn't have native barriers) */
#if PLATFORM_MACOS || PLATFORM_WINDOWS
    typedef struct {
        pthread_mutex_t mutex;
        pthread_cond_t cond;
        int count;
        int trip_count;
        unsigned int generation;
    } pthread_barrier_t;

    typedef struct {
        int dummy;
    } pthread_barrierattr_t;

    int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count);
    int pthread_barrier_destroy(pthread_barrier_t *barrier);
    int pthread_barrier_wait(pthread_barrier_t *barrier);
#endif

/* Sieve of Eratosthenes workload */
/* Returns number of primes found, fills primes array if provided */
uint64_t run_prime_sieve(uint64_t limit, bool *result_array, uint64_t *primes_found);

/* Verify that prime sieve produced correct result */
bool verify_prime_sieve_result(uint64_t primes_found);

/* Matrix multiplication workload */
/* Allocates and deallocates its own memory */
double run_matrix_multiplication(int size);

/* Verify matrix multiplication checksum (non-zero indicates valid computation) */
bool verify_matrix_result(double checksum);

/* Thread-local workload data structures */
typedef struct {
    int thread_id;
    int matrix_size;
    double checksum;    /* Result to prevent dead code elimination */
    double elapsed_ms;  /* Time taken for the workload */
    bool success;
} thread_workload_result_t;

/* Workload types for multi-threaded runs */
typedef enum {
    WORKLOAD_SIEVE,
    WORKLOAD_MATRIX
} workload_type_t;

/* Thread function for running workloads in multi-core mode */
void *thread_workload_worker(void *arg);

/* Thread argument structure */
typedef struct {
    int thread_id;
    int cpu_id;
    workload_type_t workload;
    bool verbose;
    thread_workload_result_t *result;
    pthread_barrier_t *barrier;
} thread_arg_t;

#endif /* WORKLOADS_H */
