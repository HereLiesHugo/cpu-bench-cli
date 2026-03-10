#ifndef SCORE_H
#define SCORE_H

#include "common.h"
#include "workloads.h"

/* Calculate scaling efficiency percentage */
/* Formula: (single_core_time / (multi_core_time * num_threads)) * 100 */
double calculate_efficiency(double single_time, double multi_time, int num_threads);

/* Output formatting functions */
void print_text_results(const benchmark_results_t *results);
void print_json_results(const benchmark_results_t *results);

/* Print verbose iteration timing */
void print_verbose_iteration(const char *workload_name, int iteration, double elapsed_ms);

/* Utility formatting functions */
const char *workload_name(workload_type_t type);
const char *mode_name(bool is_multi);

#endif /* SCORE_H */
