#include "sysinfo.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Platform-specific includes */
#if PLATFORM_MACOS
    #include <sys/sysctl.h>
    #include <mach/mach.h>
    #include <mach/mach_host.h>
#elif PLATFORM_LINUX
    #include <sys/sysinfo.h>
#endif

/* Cross-platform logical core count */
int get_logical_cores(void) {
    #if PLATFORM_WINDOWS
        SYSTEM_INFO sysinfo;
        GetSystemInfo(&sysinfo);
        return sysinfo.dwNumberOfProcessors;
    #else
        long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
        return (nprocs > 0) ? (int)nprocs : 1;
    #endif
}

/* Get physical core count estimation */
int get_physical_core_count(void) {
    int logical = get_logical_cores();
    
    #if PLATFORM_MACOS
        int physical = 0;
        size_t len = sizeof(physical);
        if (sysctlbyname("hw.physicalcpu", &physical, &len, NULL, 0) == 0) {
            return physical;
        }
        return logical / 2;  /* Estimate: assume hyperthreading */
    #elif PLATFORM_LINUX
        /* Try to read from cpuinfo */
        FILE *fp = fopen("/proc/cpuinfo", "r");
        if (fp) {
            char line[256];
            int cores_per_socket = 0;
            int sockets = 0;
            int max_socket_id = -1;
            
            while (fgets(line, sizeof(line), fp)) {
                int socket_id;
                if (sscanf(line, "physical id : %d", &socket_id) == 1) {
                    if (socket_id > max_socket_id) {
                        max_socket_id = socket_id;
                    }
                }
                int cores;
                if (sscanf(line, "cpu cores : %d", &cores) == 1) {
                    if (cores > cores_per_socket) {
                        cores_per_socket = cores;
                    }
                }
            }
            fclose(fp);
            
            sockets = max_socket_id + 1;
            if (cores_per_socket > 0 && sockets > 0) {
                return cores_per_socket * sockets;
            }
        }
        return logical / 2;  /* Estimate */
    #else
        return logical / 2;  /* Conservative estimate */
    #endif
}

/* macOS implementations */
#if PLATFORM_MACOS
void detect_macos_cpu_info(sys_info_t *info) {
    size_t len = sizeof(info->cpu_model);
    if (sysctlbyname("machdep.cpu.brand_string", info->cpu_model, &len, NULL, 0) != 0) {
        strcpy(info->cpu_model, "Unknown Apple CPU");
    }
    
    info->logical_cores = get_logical_cores();
    info->physical_cores = get_physical_core_count();
    
    /* No governor control on macOS */
    strcpy(info->governor, "n/a");
    info->governor_performance = true;  /* Assume OK on macOS */
    
    info->baseline_temp = read_macos_temperature();
}

int read_macos_temperature(void) {
    /* macOS thermal monitoring - try to read SMC temperature */
    int temp = 0;
    size_t len = sizeof(temp);
    
    /* Try various thermal keys */
    if (sysctlbyname("machdep.xcpm.cpu_thermal_level", &temp, &len, NULL, 0) != 0) {
        /* Fallback: return 0 to indicate unavailable */
        temp = 0;
    }
    return temp;
}
#endif

/* Linux implementations */
#if PLATFORM_LINUX
void detect_linux_cpu_info(sys_info_t *info) {
    /* Read CPU model from /proc/cpuinfo */
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (fp) {
        char line[256];
        info->cpu_model[0] = '\0';
        while (fgets(line, sizeof(line), fp)) {
            if (strncmp(line, "model name", 10) == 0) {
                char *colon = strchr(line, ':');
                if (colon) {
                    strncpy(info->cpu_model, colon + 2, sizeof(info->cpu_model) - 1);
                    info->cpu_model[sizeof(info->cpu_model) - 1] = '\0';
                    /* Remove newline */
                    size_t len = strlen(info->cpu_model);
                    if (len > 0 && info->cpu_model[len-1] == '\n') {
                        info->cpu_model[len-1] = '\0';
                    }
                    break;
                }
            }
        }
        fclose(fp);
    }
    
    if (info->cpu_model[0] == '\0') {
        strcpy(info->cpu_model, "Unknown CPU");
    }
    
    info->logical_cores = get_logical_cores();
    info->physical_cores = get_physical_core_count();
    
    /* Check governor */
    check_linux_governor(info);
    
    /* Read temperature */
    info->baseline_temp = read_linux_temperature();
}

void check_linux_governor(sys_info_t *info) {
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/scaling_governor", "r");
    if (fp) {
        if (fgets(info->governor, sizeof(info->governor), fp)) {
            /* Remove newline */
            size_t len = strlen(info->governor);
            if (len > 0 && info->governor[len-1] == '\n') {
                info->governor[len-1] = '\0';
            }
            info->governor_performance = (strcmp(info->governor, "performance") == 0);
        } else {
            strcpy(info->governor, "unknown");
            info->governor_performance = false;
        }
        fclose(fp);
    } else {
        strcpy(info->governor, "unavailable");
        info->governor_performance = true;  /* Can't check, assume OK */
    }
}

int read_linux_temperature(void) {
    FILE *fp = fopen("/sys/class/thermal/thermal_zone0/temp", "r");
    int temp = 0;
    if (fp) {
        fscanf(fp, "%d", &temp);
        fclose(fp);
    }
    return temp;
}

double read_linux_load_avg(void) {
    FILE *fp = fopen("/proc/loadavg", "r");
    double load = 0.0;
    if (fp) {
        fscanf(fp, "%lf", &load);
        fclose(fp);
    }
    return load;
}
#endif

/* Windows implementations */
#if PLATFORM_WINDOWS
#include <windows.h>

void detect_windows_cpu_info(sys_info_t *info) {
    /* Get CPU name from registry */
    HKEY hKey;
    DWORD bufSize = sizeof(info->cpu_model);
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
        "HARDWARE\\DESCRIPTION\\System\\CentralProcessor\\0",
        0, KEY_QUERY_VALUE, &hKey) == ERROR_SUCCESS) {
        RegQueryValueExA(hKey, "ProcessorNameString", NULL, NULL, 
                        (LPBYTE)info->cpu_model, &bufSize);
        RegCloseKey(hKey);
    } else {
        strcpy(info->cpu_model, "Unknown Windows CPU");
    }
    
    SYSTEM_INFO sysinfo;
    GetSystemInfo(&sysinfo);
    info->logical_cores = sysinfo.dwNumberOfProcessors;
    
    /* Estimate physical cores */
    info->physical_cores = info->logical_cores / 2;
    if (info->physical_cores < 1) info->physical_cores = 1;
    
    /* No governor control on Windows */
    strcpy(info->governor, "n/a");
    info->governor_performance = true;
    
    info->baseline_temp = read_windows_temperature();
}

int read_windows_temperature(void) {
    /* Windows thermal monitoring requires WMI - simplified here */
    /* Return 0 to indicate unavailable in basic implementation */
    return 0;
}

double read_windows_load_avg(void) {
    /* Windows doesn't have traditional load average */
    return 0.0;
}
#endif

/* Main detection function */
void detect_system_info(sys_info_t *info) {
    memset(info, 0, sizeof(sys_info_t));
    
    #if PLATFORM_MACOS
        detect_macos_cpu_info(info);
    #elif PLATFORM_LINUX
        detect_linux_cpu_info(info);
    #elif PLATFORM_WINDOWS
        detect_windows_cpu_info(info);
    #else
        strcpy(info->cpu_model, "Unknown Platform");
        info->logical_cores = get_logical_cores();
        info->physical_cores = info->logical_cores;
        strcpy(info->governor, "unknown");
        info->governor_performance = true;
    #endif
    
    /* Read load average */
    #if PLATFORM_LINUX
        info->load_avg[0] = read_linux_load_avg();
    #elif PLATFORM_WINDOWS
        info->load_avg[0] = read_windows_load_avg();
    #else
        info->load_avg[0] = 0.0;
    #endif
}

/* Read current temperature (cross-platform) */
int read_current_temperature(void) {
    #if PLATFORM_LINUX
        return read_linux_temperature();
    #elif PLATFORM_MACOS
        return read_macos_temperature();
    #elif PLATFORM_WINDOWS
        return read_windows_temperature();
    #else
        return 0;
    #endif
}

/* Check thermal governor status */
bool check_thermal_governor(void) {
    sys_info_t info;
    detect_system_info(&info);
    return info.governor_performance;
}

/* Print system information */
void print_system_info(const sys_info_t *info) {
    printf("System Information:\n");
    printf("  CPU Model:        %s\n", info->cpu_model);
    printf("  Logical Cores:    %d\n", info->logical_cores);
    printf("  Physical Cores:   %d (estimated)\n", info->physical_cores);
    
    if (strcmp(info->governor, "n/a") != 0 && strcmp(info->governor, "unknown") != 0) {
        printf("  CPU Governor:     %s\n", info->governor);
    }
    
    if (info->baseline_temp > 0) {
        printf("  Baseline Temp:    %.1f°C\n", info->baseline_temp / 1000.0);
    }
    
    printf("\n");
}

/* Print health warnings */
void print_health_warnings(int temp_delta, bool thermal_warn, double load_before, bool load_warn) {
    bool printed_header = false;
    
    if (thermal_warn) {
        if (!printed_header) {
            printf("\nHealth Warnings:\n");
            printed_header = true;
        }
        printf("  ⚠ WARNING: Temperature increased by %d°C during benchmark.\n", temp_delta);
        printf("    Thermal throttling may have affected results.\n");
    }
    
    if (load_warn) {
        if (!printed_header) {
            printf("\nHealth Warnings:\n");
            printed_header = true;
        }
        printf("  ⚠ WARNING: System load (%.2f) exceeds threshold (%.1f).\n", 
               load_before, LOAD_WARNING_THRESHOLD);
        printf("    Background processes may skew results.\n");
    }
}
