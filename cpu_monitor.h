#ifndef CPU_MONITOR_H
#define CPU_MONITOR_H

#define _POSIX_C_SOURCE 200809L

#include <stddef.h>
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    unsigned long long field[10]; // user nice system idle iowait irq softirq steal guest guest_nice
    int nfields;
} CpuTimes;

typedef struct {
    char *label;
    CpuTimes times;
} CpuLine;

typedef struct {
    CpuLine *arr;
    size_t count;
    size_t cap;
} CpuSet;

typedef struct {
    double total_usage;
    double *core_usage;
    size_t core_count;
} CpuUsageData;

// Основные функции
void cpu_monitor_init(void);
void cpu_monitor_cleanup(void);
int cpu_monitor_get_usage(CpuUsageData *data);
size_t cpu_monitor_get_core_count(void);

// UDP клиент функции
void cpu_udp_client_start(void);
void cpu_udp_client_stop(void);
int cpu_udp_client_is_running(void);

#ifdef __cplusplus
}
#endif

#endif // CPU_MONITOR_H
