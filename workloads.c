#include "workloads.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <stdint.h>
#include <math.h>

/* Cross-platform pthread barrier implementation for macOS/Windows */
#if PLATFORM_MACOS || PLATFORM_WINDOWS
int pthread_barrier_init(pthread_barrier_t *barrier, const pthread_barrierattr_t *attr, unsigned int count) {
    if (count == 0) {
        return -1;
    }
    if (pthread_mutex_init(&barrier->mutex, NULL) != 0) {
        return -1;
    }
    if (pthread_cond_init(&barrier->cond, NULL) != 0) {
        pthread_mutex_destroy(&barrier->mutex);
        return -1;
    }
    barrier->count = 0;
    barrier->trip_count = count;
    barrier->generation = 0;
    (void)attr;  /* Unused */
    return 0;
}

int pthread_barrier_destroy(pthread_barrier_t *barrier) {
    pthread_cond_destroy(&barrier->cond);
    pthread_mutex_destroy(&barrier->mutex);
    return 0;
}

int pthread_barrier_wait(pthread_barrier_t *barrier) {
    pthread_mutex_lock(&barrier->mutex);
    
    barrier->count++;
    if (barrier->count >= barrier->trip_count) {
        /* Last thread - wake everyone up */
        barrier->count = 0;
        barrier->generation++;
        pthread_cond_broadcast(&barrier->cond);
        pthread_mutex_unlock(&barrier->mutex);
        return 1;  /* Returns 1 to one thread (the last one) */
    } else {
        /* Not the last thread - wait */
        unsigned int gen = barrier->generation;
        do {
            pthread_cond_wait(&barrier->cond, &barrier->mutex);
        } while (gen == barrier->generation);  /* Spurious wakeups protection */
        pthread_mutex_unlock(&barrier->mutex);
        return 0;
    }
}
#endif

/* External volatile variable to prevent dead code elimination */
volatile uint64_t g_primes_count = 0;
volatile double g_matrix_checksum = 0.0;

/* Sieve of Eratosthenes implementation */
uint64_t run_prime_sieve(uint64_t limit, bool *result_array, uint64_t *primes_found) {
    /* Allocate sieve array if not provided */
    bool *sieve = result_array;
    bool need_free = false;
    
    if (sieve == NULL) {
        sieve = calloc(limit + 1, sizeof(bool));
        if (!sieve) {
            fprintf(stderr, "Error: Failed to allocate memory for sieve\n");
            exit(1);
        }
        need_free = true;
    }
    
    /* Initialize: 0 and 1 are not prime */
    if (limit >= 0) sieve[0] = false;
    if (limit >= 1) sieve[1] = false;
    
    /* Initialize rest to true */
    for (uint64_t i = 2; i <= limit; i++) {
        sieve[i] = true;
    }
    
    /* Sieve algorithm */
    uint64_t sqrt_limit = (uint64_t)sqrt((double)limit) + 1;
    
    for (uint64_t p = 2; p <= sqrt_limit; p++) {
        if (sieve[p]) {
            /* Mark all multiples of p as not prime */
            for (uint64_t multiple = p * p; multiple <= limit; multiple += p) {
                sieve[multiple] = false;
            }
        }
    }
    
    /* Count primes */
    uint64_t count = 0;
    for (uint64_t i = 2; i <= limit; i++) {
        if (sieve[i]) {
            count++;
        }
    }
    
    /* Store in global volatile to prevent optimization */
    g_primes_count = count;
    
    if (primes_found) {
        *primes_found = count;
    }
    
    if (need_free) {
        free(sieve);
    }
    
    return count;
}

/* Verify prime sieve result */
bool verify_prime_sieve_result(uint64_t primes_found) {
    return (primes_found == SIEVE_EXPECTED_PRIMES);
}

/* Naive dense matrix multiplication */
double run_matrix_multiplication(int size) {
    /* Allocate matrices locally (not passed from main to avoid NUMA penalties) */
    double **A = malloc(size * sizeof(double *));
    double **B = malloc(size * sizeof(double *));
    double **C = malloc(size * sizeof(double *));
    
    if (!A || !B || !C) {
        fprintf(stderr, "Error: Failed to allocate matrix row pointers\n");
        exit(1);
    }
    
    /* Allocate rows */
    for (int i = 0; i < size; i++) {
        A[i] = malloc(size * sizeof(double));
        B[i] = malloc(size * sizeof(double));
        C[i] = calloc(size, sizeof(double));  /* C must be zero-initialized */
        
        if (!A[i] || !B[i] || !C[i]) {
            fprintf(stderr, "Error: Failed to allocate matrix rows\n");
            exit(1);
        }
    }
    
    /* Initialize matrices with deterministic values */
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            A[i][j] = (double)(i + j) * 0.5;
            B[i][j] = (double)(i - j) * 0.5;
        }
    }
    
    /* Naive triple-nested loop matrix multiplication */
    for (int i = 0; i < size; i++) {
        for (int k = 0; k < size; k++) {
            double a_ik = A[i][k];
            for (int j = 0; j < size; j++) {
                C[i][j] += a_ik * B[k][j];
            }
        }
    }
    
    /* Calculate checksum to verify computation and prevent dead code elimination */
    double checksum = 0.0;
    for (int i = 0; i < size; i++) {
        for (int j = 0; j < size; j++) {
            checksum += C[i][j];
            checksum = fmod(checksum + 1000.0, 1000000.0);  /* Keep manageable */
        }
    }
    
    /* Store in global volatile */
    g_matrix_checksum = checksum;
    
    /* Free memory */
    for (int i = 0; i < size; i++) {
        free(A[i]);
        free(B[i]);
        free(C[i]);
    }
    free(A);
    free(B);
    free(C);
    
    return checksum;
}

/* Verify matrix result (non-zero indicates valid computation) */
bool verify_matrix_result(double checksum) {
    return (checksum != 0.0 && !isnan(checksum) && !isinf(checksum));
}

/* Thread workload worker */
void *thread_workload_worker(void *arg) {
    thread_arg_t *thread_arg = (thread_arg_t *)arg;
    thread_workload_result_t *result = thread_arg->result;
    
    result->thread_id = thread_arg->thread_id;
    result->success = false;
    
    /* Wait at barrier for synchronized start */
    if (thread_arg->barrier) {
        pthread_barrier_wait(thread_arg->barrier);
    }
    
    /* Run the workload */
    if (thread_arg->workload == WORKLOAD_SIEVE) {
        uint64_t primes_found = 0;
        run_prime_sieve(SIEVE_LIMIT, NULL, &primes_found);
        result->success = verify_prime_sieve_result(primes_found);
        result->checksum = (double)primes_found;  /* Store as checksum for consistency */
    } else {  /* WORKLOAD_MATRIX */
        double checksum = run_matrix_multiplication(MATRIX_SIZE);
        result->success = verify_matrix_result(checksum);
        result->checksum = checksum;
    }
    
    return result;
}
