#include "score.h"
#include "workloads.h"
#include <stdio.h>
#include <math.h>

/* Calculate scaling efficiency percentage */
double calculate_efficiency(double single_time, double multi_time, int num_threads) {
    if (multi_time <= 0 || num_threads <= 0) {
        return 0.0;
    }
    return (single_time / (multi_time * num_threads)) * 100.0;
}

/* Get workload name string */
const char *workload_name(workload_type_t type) {
    return (type == WORKLOAD_SIEVE) ? "Prime Sieve" : "Matrix Multiply";
}

/* Get mode name string */
const char *mode_name(bool is_multi) {
    return is_multi ? "Multi-Core" : "Single-Core";
}

/* Print verbose iteration timing */
void print_verbose_iteration(const char *workload_name, int iteration, double elapsed_ms) {
    (void)workload_name;  /* Unused but kept for API consistency */
    printf("    Iteration %d: %.3f ms\n", iteration, elapsed_ms);
}

/* Print results in text format */
void print_text_results(const benchmark_results_t *results) {
    printf("\n");
    printf("╔══════════════════════════════════════════════════════════════╗\n");
    printf("║              CPU BENCHMARK RESULTS                           ║\n");
    printf("╚══════════════════════════════════════════════════════════════╝\n");
    printf("\n");
    
    /* System info */
    printf("CPU: %s\n", results->cpu_model);
    printf("Cores: %d logical, %d physical\n", results->logical_cores, results->physical_cores);
    printf("\n");
    
    /* Single-core results */
    printf("Single-Core Benchmarks:\n");
    printf("  Prime Sieve:     %8.3f ms\n", results->single_sieve.elapsed_ms);
    printf("  Matrix Multiply: %8.3f ms\n", results->single_matrix.elapsed_ms);
    printf("\n");
    
    /* Multi-core results */
    if (results->threads_used > 0) {
        printf("Multi-Core Benchmarks (%d threads):\n", results->threads_used);
        printf("  Prime Sieve:     %8.3f ms\n", results->multi_sieve.elapsed_ms);
        printf("  Matrix Multiply: %8.3f ms\n", results->multi_matrix.elapsed_ms);
        printf("\n");
        
        /* Efficiency calculations */
        printf("Scaling Efficiency:\n");
        double sieve_eff = calculate_efficiency(results->single_sieve.elapsed_ms,
                                                results->multi_sieve.elapsed_ms,
                                                results->threads_used);
        double matrix_eff = calculate_efficiency(results->single_matrix.elapsed_ms,
                                                 results->multi_matrix.elapsed_ms,
                                                 results->threads_used);
        
        printf("  Prime Sieve:     %6.1f%%\n", sieve_eff);
        printf("  Matrix Multiply: %6.1f%%\n", matrix_eff);
        printf("\n");
    }
    
    /* Health warnings */
    if (results->thermal_warning) {
        printf("⚠ WARNING: Temperature increased by %d°C during benchmark.\n", results->temp_delta);
        printf("  Thermal throttling may have affected results.\n");
    }
    
    if (results->load_warning) {
        printf("⚠ WARNING: System load (%.2f) exceeds threshold (%.1f).\n", 
               results->load_before, LOAD_WARNING_THRESHOLD);
        printf("  Background processes may skew results.\n");
    }
}

/* Print results in JSON format */
void print_json_results(const benchmark_results_t *results) {
    double sieve_eff = calculate_efficiency(results->single_sieve.elapsed_ms,
                                             results->multi_sieve.elapsed_ms,
                                             results->threads_used);
    double matrix_eff = calculate_efficiency(results->single_matrix.elapsed_ms,
                                              results->multi_matrix.elapsed_ms,
                                              results->threads_used);
    
    printf("{\n");
    printf("  \"system\": {\n");
    printf("    \"cpu_model\": \"%s\",\n", results->cpu_model);
    printf("    \"logical_cores\": %d,\n", results->logical_cores);
    printf("    \"physical_cores\": %d\n", results->physical_cores);
    printf("  },\n");
    
    printf("  \"single_core\": {\n");
    printf("    \"prime_sieve_ms\": %.3f,\n", results->single_sieve.elapsed_ms);
    printf("    \"matrix_multiply_ms\": %.3f\n", results->single_matrix.elapsed_ms);
    printf("  },\n");
    
    printf("  \"multi_core\": {\n");
    printf("    \"threads\": %d,\n", results->threads_used);
    printf("    \"prime_sieve_ms\": %.3f,\n", results->multi_sieve.elapsed_ms);
    printf("    \"matrix_multiply_ms\": %.3f\n", results->multi_matrix.elapsed_ms);
    printf("  },\n");
    
    printf("  \"efficiency\": {\n");
    printf("    \"prime_sieve_pct\": %.2f,\n", sieve_eff);
    printf("    \"matrix_multiply_pct\": %.2f\n", matrix_eff);
    printf("  },\n");
    
    printf("  \"warnings\": {\n");
    printf("    \"thermal_delta_c\": %d,\n", results->temp_delta);
    printf("    \"thermal_warning\": %s,\n", results->thermal_warning ? "true" : "false");
    printf("    \"load_before\": %.2f,\n", results->load_before);
    printf("    \"load_warning\": %s\n", results->load_warning ? "true" : "false");
    printf("  }\n");
    
    printf("}\n");
}
