#ifndef SYSINFO_H
#define SYSINFO_H

#include "common.h"

/* System information structure */
typedef struct {
    char cpu_model[256];
    int logical_cores;
    int physical_cores;
    char governor[64];
    int baseline_temp;      /* Temperature in millidegrees Celsius */
    double load_avg[3];   /* 1, 5, 15 minute load averages */
    bool governor_performance;
} sys_info_t;

/* Detect system information */
void detect_system_info(sys_info_t *info);

/* Get number of logical cores */
int get_logical_cores(void);

/* Health checks */
int read_current_temperature(void);
double read_load_average(void);
bool check_thermal_governor(void);

/* Display system info */
void print_system_info(const sys_info_t *info);
void print_health_warnings(int temp_delta, bool thermal_warn, double load_before, bool load_warn);

/* Platform-specific implementations */
#if PLATFORM_MACOS
    void detect_macos_cpu_info(sys_info_t *info);
    int read_macos_temperature(void);
#elif PLATFORM_WINDOWS
    void detect_windows_cpu_info(sys_info_t *info);
    int read_windows_temperature(void);
    double read_windows_load_avg(void);
#elif PLATFORM_LINUX
    void detect_linux_cpu_info(sys_info_t *info);
    int read_linux_temperature(void);
    double read_linux_load_avg(void);
    void check_linux_governor(sys_info_t *info);
#endif

#endif /* SYSINFO_H */
