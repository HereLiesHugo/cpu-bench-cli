#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <unistd.h>
#include <time.h>

#include "common.h"
#include "sysinfo.h"
#include "workloads.h"
#include "bench.h"
#include "score.h"

/* Print usage information */
static void print_usage(const char *program_name) {
    printf("Usage: %s [OPTIONS]\n\n", program_name);
    printf("CPU Benchmark Tool - Measures single and multi-core performance\n\n");
    printf("Options:\n");
    printf("  --single          Run single-core benchmark only\n");
    printf("  --multi           Run multi-core benchmark only\n");
    printf("  --all             Run both single and multi-core benchmarks (default)\n");
    printf("  --threads N       Override thread count for multi-core benchmark\n");
    printf("  --json            Output results in JSON format\n");
    printf("  --verbose         Print detailed timing per iteration\n");
    printf("  --help            Display this help message\n");
    printf("\n");
    printf("Workloads:\n");
    printf("  - Prime Sieve (Sieve of Eratosthenes to find primes below %d)\n", SIEVE_LIMIT);
    printf("  - Matrix Multiply (%dx%d dense double-precision matrices)\n", MATRIX_SIZE, MATRIX_SIZE);
    printf("\n");
    printf("Examples:\n");
    printf("  %s --all --verbose          Run all benchmarks with detailed output\n", program_name);
    printf("  %s --single --json          Single-core benchmark, JSON output\n", program_name);
    printf("  %s --multi --threads 4      Multi-core with 4 threads\n", program_name);
}

/* Parse command-line arguments */
static bool parse_arguments(int argc, char *argv[], benchmark_options_t *options) {
    /* Set defaults */
    options->single_mode = false;
    options->multi_mode = false;
    options->thread_count = 0;  /* Auto-detect */
    options->json_output = false;
    options->verbose = false;
    
    /* Check if no mode specified - default to --all */
    bool mode_specified = false;
    
    static struct option long_options[] = {
        {"single",  no_argument,       0, 's'},
        {"multi",   no_argument,       0, 'm'},
        {"all",     no_argument,       0, 'a'},
        {"threads", required_argument, 0, 't'},
        {"json",    no_argument,       0, 'j'},
        {"verbose", no_argument,       0, 'v'},
        {"help",    no_argument,       0, 'h'},
        {0, 0, 0, 0}
    };
    
    int opt;
    int option_index = 0;
    
    while ((opt = getopt_long(argc, argv, "smat:jvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 's':
                options->single_mode = true;
                mode_specified = true;
                break;
            case 'm':
                options->multi_mode = true;
                mode_specified = true;
                break;
            case 'a':
                options->single_mode = true;
                options->multi_mode = true;
                mode_specified = true;
                break;
            case 't':
                options->thread_count = atoi(optarg);
                if (options->thread_count <= 0) {
                    fprintf(stderr, "Error: Invalid thread count: %s\n", optarg);
                    return false;
                }
                break;
            case 'j':
                options->json_output = true;
                break;
            case 'v':
                options->verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(0);
            default:
                print_usage(argv[0]);
                return false;
        }
    }
    
    /* Default to --all if no mode specified */
    if (!mode_specified) {
        options->single_mode = true;
        options->multi_mode = true;
    }
    
    return true;
}

/* Run health checks and return results */
/* Note: This functionality is integrated directly in main() */
/* Keeping prototype for potential future use */

int main(int argc, char *argv[]) {
    benchmark_options_t options;
    benchmark_results_t results = {0};
    sys_info_t sys_info;
    
    /* Parse command-line arguments */
    if (!parse_arguments(argc, argv, &options)) {
        return 1;
    }
    
    /* Detect system information */
    detect_system_info(&sys_info);
    
    /* Copy to results */
    strncpy(results.cpu_model, sys_info.cpu_model, sizeof(results.cpu_model) - 1);
    results.logical_cores = sys_info.logical_cores;
    results.physical_cores = sys_info.physical_cores;
    
    /* Determine thread count for multi-core benchmark */
    int num_threads = options.thread_count;
    if (num_threads == 0) {
        num_threads = results.physical_cores;
        if (num_threads < 1) num_threads = 1;
    }
    results.threads_used = (options.multi_mode) ? num_threads : 0;
    
    /* Print system info (unless JSON output requested) */
    if (!options.json_output) {
        print_system_info(&sys_info);
        
        /* Check and warn about governor */
        if (!sys_info.governor_performance && strcmp(sys_info.governor, "unavailable") != 0 
            && strcmp(sys_info.governor, "n/a") != 0) {
            printf("⚠ WARNING: CPU governor is '%s', not 'performance'.\n", sys_info.governor);
            printf("   Consider: echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor\n\n");
        }
        
        /* Warn about high load */
        if (sys_info.load_avg[0] > LOAD_WARNING_THRESHOLD) {
            printf("⚠ WARNING: System load (%.2f) exceeds threshold (%.1f).\n", 
                   sys_info.load_avg[0], LOAD_WARNING_THRESHOLD);
            printf("   Background processes may skew results.\n\n");
        }
    }
    
    /* Store baseline temperature */
    int baseline_temp = sys_info.baseline_temp;
    
    /* Run benchmarks */
    if (options.single_mode) {
        if (!options.json_output) {
            printf("Running Single-Core Benchmarks...\n");
        }
        
        results.single_sieve = run_single_core_benchmark(WORKLOAD_SIEVE, options.verbose);
        results.single_matrix = run_single_core_benchmark(WORKLOAD_MATRIX, options.verbose);
        
        if (!options.json_output) {
            printf("  Prime Sieve:     %.3f ms\n", results.single_sieve.elapsed_ms);
            printf("  Matrix Multiply: %.3f ms\n", results.single_matrix.elapsed_ms);
        }
    }
    
    if (options.multi_mode) {
        if (!options.json_output) {
            printf("\nRunning Multi-Core Benchmarks with %d threads...\n", num_threads);
        }
        
        results.multi_sieve = run_multi_core_benchmark(WORKLOAD_SIEVE, num_threads, options.verbose);
        results.multi_matrix = run_multi_core_benchmark(WORKLOAD_MATRIX, num_threads, options.verbose);
        
        if (!options.json_output) {
            printf("  Prime Sieve:     %.3f ms\n", results.multi_sieve.elapsed_ms);
            printf("  Matrix Multiply: %.3f ms\n", results.multi_matrix.elapsed_ms);
        }
    }
    
    /* Post-benchmark health checks */
    int final_temp = read_current_temperature();
    if (baseline_temp > 0 && final_temp > 0) {
        results.temp_delta = (final_temp - baseline_temp) / 1000;  /* Convert from millidegrees */
        results.thermal_warning = (results.temp_delta > THERMAL_WARNING_DELTA);
    }
    
    /* Output results */
    if (options.json_output) {
        print_json_results(&results);
    } else {
        print_text_results(&results);
        
        /* Print thermal warning if applicable */
        if (results.thermal_warning) {
            printf("\n⚠ WARNING: Temperature increased by %d°C during benchmark.\n", results.temp_delta);
            printf("   Thermal throttling may have affected results.\n");
        }
    }
    
    return 0;
}
