#include "bench.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Platform-specific includes for CPU pinning */
#if PLATFORM_MACOS
    #include <mach/mach.h>
    #include <mach/thread_policy.h>
    #include <mach/thread_act.h>
#elif PLATFORM_LINUX
    #include <sched.h>
#endif

/* External globals for result verification */
extern volatile uint64_t g_primes_count;
extern volatile double g_matrix_checksum;

/* Pin thread to specific CPU (cross-platform) */
bool pin_thread_to_cpu(int cpu_id) {
    #if PLATFORM_LINUX
        cpu_set_t cpuset;
        CPU_ZERO(&cpuset);
        CPU_SET(cpu_id, &cpuset);
        
        pthread_t current_thread = pthread_self();
        int ret = pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
        return (ret == 0);
    #elif PLATFORM_MACOS
        /* macOS uses thread affinity policy */
        thread_port_t thread = pthread_mach_thread_np(pthread_self());
        thread_affinity_policy_data_t policy;
        policy.affinity_tag = cpu_id;
        
        kern_return_t ret = thread_policy_set(thread, THREAD_AFFINITY_POLICY,
                                             (thread_policy_t)&policy, 
                                             THREAD_AFFINITY_POLICY_COUNT);
        return (ret == KERN_SUCCESS);
    #elif PLATFORM_WINDOWS
        /* Windows: use SetThreadAffinityMask */
        DWORD_PTR mask = (1ULL << cpu_id);
        return (SetThreadAffinityMask(GetCurrentThread(), mask) != 0);
    #else
        (void)cpu_id;
        return false;
    #endif
}

/* Run a single iteration of workload and return elapsed time */
static double run_workload_iteration(workload_type_t workload, bool verbose, int iteration_num) {
    bench_timer_t timer;
    
    TIMER_START(timer);
    
    if (workload == WORKLOAD_SIEVE) {
        uint64_t primes_found = 0;
        run_prime_sieve(SIEVE_LIMIT, NULL, &primes_found);
        
        /* Verify result */
        if (!verify_prime_sieve_result(primes_found)) {
            fprintf(stderr, "ERROR: Prime sieve validation failed! Expected %d, got %lu\n",
                    SIEVE_EXPECTED_PRIMES, (unsigned long)primes_found);
            exit(1);
        }
    } else {  /* WORKLOAD_MATRIX */
        double checksum = run_matrix_multiplication(MATRIX_SIZE);
        
        /* Verify result */
        if (!verify_matrix_result(checksum)) {
            fprintf(stderr, "ERROR: Matrix multiplication validation failed! Invalid checksum\n");
            exit(1);
        }
    }
    
    TIMER_STOP(timer);
    double elapsed = TIMER_MS(timer);
    
    if (verbose) {
        printf("    Iteration %d: %.3f ms\n", iteration_num, elapsed);
    }
    
    return elapsed;
}

/* Single-core benchmark runner */
workload_result_t run_single_core_benchmark(workload_type_t workload, bool verbose) {
    workload_result_t result = {0};
    const char *workload_name_str = (workload == WORKLOAD_SIEVE) ? "Prime Sieve" : "Matrix Mult";
    
    if (verbose) {
        printf("  Running single-core %s benchmark...\n", workload_name_str);
    }
    
    /* Pin to core 0 */
    if (!pin_thread_to_cpu(0)) {
        if (verbose) {
            printf("    Warning: Failed to pin thread to core 0\n");
        }
    }
    
    /* Warmup iteration (discarded) */
    if (verbose) {
        printf("    Warmup iteration...\n");
    }
    run_workload_iteration(workload, false, 0);
    
    /* Timed iterations */
    double total_ms = 0.0;
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        total_ms += run_workload_iteration(workload, verbose, i + 1);
    }
    
    result.elapsed_ms = total_ms / NUM_ITERATIONS;
    result.iterations = NUM_ITERATIONS;
    result.valid = true;
    
    if (verbose) {
        printf("    Average: %.3f ms\n", result.elapsed_ms);
    }
    
    return result;
}

/* Thread worker for multi-core benchmark */
void *benchmark_thread_worker(void *arg) {
    bench_thread_data_t *data = (bench_thread_data_t *)arg;
    
    /* Pin to assigned CPU */
    if (!pin_thread_to_cpu(data->cpu_id)) {
        if (data->verbose) {
            printf("    Thread %d: Warning - failed to pin to CPU %d\n", 
                   data->thread_id, data->cpu_id);
        }
    }
    
    /* Allocate local working memory for matrix workload */
    if (data->workload == WORKLOAD_MATRIX) {
        /* Memory is allocated inside run_matrix_multiplication */
    }
    
    /* Wait at barrier for synchronized start */
    pthread_barrier_wait(data->barrier);
    
    /* Run warmup (discarded) */
    if (data->workload == WORKLOAD_SIEVE) {
        uint64_t primes = 0;
        run_prime_sieve(SIEVE_LIMIT, NULL, &primes);
    } else {
        run_matrix_multiplication(MATRIX_SIZE);
    }
    
    /* Wait at barrier again for synchronized restart */
    pthread_barrier_wait(data->barrier);
    
    /* Run timed iterations */
    bench_timer_t timer;
    TIMER_START(timer);
    
    for (int i = 0; i < NUM_ITERATIONS; i++) {
        if (data->workload == WORKLOAD_SIEVE) {
            uint64_t primes_found = 0;
            run_prime_sieve(SIEVE_LIMIT, NULL, &primes_found);
            data->result.success = verify_prime_sieve_result(primes_found);
            data->result.checksum = (double)primes_found;
        } else {
            double checksum = run_matrix_multiplication(MATRIX_SIZE);
            data->result.success = verify_matrix_result(checksum);
            data->result.checksum = checksum;
        }
        
        if (!data->result.success) {
            fprintf(stderr, "ERROR: Thread %d validation failed!\n", data->thread_id);
            break;
        }
    }
    
    TIMER_STOP(timer);
    data->result.elapsed_ms = TIMER_MS(timer) / NUM_ITERATIONS;
    data->result.thread_id = data->thread_id;
    
    return &data->result;
}

/* Multi-core benchmark runner */
workload_result_t run_multi_core_benchmark(workload_type_t workload, int num_threads, bool verbose) {
    workload_result_t result = {0};
    const char *workload_name_str = (workload == WORKLOAD_SIEVE) ? "Prime Sieve" : "Matrix Mult";
    
    if (verbose) {
        printf("  Running multi-core %s benchmark with %d threads...\n", 
               workload_name_str, num_threads);
    }
    
    /* Allocate thread data */
    bench_thread_data_t *thread_data = calloc(num_threads, sizeof(bench_thread_data_t));
    pthread_t *threads = malloc(num_threads * sizeof(pthread_t));
    
    if (!thread_data || !threads) {
        fprintf(stderr, "Error: Failed to allocate thread data\n");
        exit(1);
    }
    
    /* Initialize barrier (2 waits: warmup and timed) */
    pthread_barrier_t barrier;
    pthread_barrier_init(&barrier, NULL, num_threads);
    
    /* Create threads */
    for (int i = 0; i < num_threads; i++) {
        thread_data[i].thread_id = i;
        thread_data[i].cpu_id = i % get_logical_cores();  /* Distribute across cores */
        thread_data[i].workload = workload;
        thread_data[i].verbose = verbose;
        thread_data[i].barrier = &barrier;
        thread_data[i].result.matrix_size = MATRIX_SIZE;
        
        int ret = pthread_create(&threads[i], NULL, benchmark_thread_worker, &thread_data[i]);
        if (ret != 0) {
            fprintf(stderr, "Error: Failed to create thread %d\n", i);
            exit(1);
        }
    }
    
    /* Wait for all threads to complete */
    double max_elapsed = 0.0;
    bool all_valid = true;
    
    for (int i = 0; i < num_threads; i++) {
        void *thread_result;
        pthread_join(threads[i], &thread_result);
        
        thread_workload_result_t *res = (thread_workload_result_t *)thread_result;
        if (res->elapsed_ms > max_elapsed) {
            max_elapsed = res->elapsed_ms;
        }
        if (!res->success) {
            all_valid = false;
        }
    }
    
    /* Cleanup */
    pthread_barrier_destroy(&barrier);
    free(thread_data);
    free(threads);
    
    /* Result is the slowest thread (wall-clock time until all complete) */
    result.elapsed_ms = max_elapsed;
    result.iterations = NUM_ITERATIONS;
    result.valid = all_valid;
    
    if (verbose) {
        printf("    Wall-clock time: %.3f ms (slowest thread)\n", result.elapsed_ms);
    }
    
    return result;
}
