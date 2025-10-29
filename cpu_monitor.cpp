#include "cpu_monitor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

static CpuSet prev_stats = {NULL, 0, 0};
static size_t core_count = 0;
static pthread_mutex_t data_mutex = PTHREAD_MUTEX_INITIALIZER;

// UDP клиент переменные
static pthread_t udp_thread;
static int udp_running = 0;
static int udp_socket = -1;

void cpuset_init(CpuSet *s) {
    s->arr = NULL;
    s->count = 0;
    s->cap = 0;
}

void cpuset_free(CpuSet *s) {
    if (!s) return;
    for (size_t i = 0; i < s->count; i++)
        free(s->arr[i].label);
    free(s->arr);
    s->arr = NULL;
    s->count = 0;
    s->cap = 0;
}

static void cpuset_push(CpuSet *s, const char *label, CpuTimes t) {
    if (s->count == s->cap) {
        size_t ncap = s->cap ? s->cap * 2 : 16;
        CpuLine *tmp = (CpuLine*)realloc(s->arr, ncap * sizeof(CpuLine)); // FIX
        if (!tmp) { perror("realloc"); exit(1); }
        s->arr = tmp;
        s->cap = ncap;
    }
    s->arr[s->count].label = strdup(label ? label : "");
    s->arr[s->count].times = t;
    s->count++;
}

static int parse_cpu_line(const char *line, char *out_label, size_t labsz, CpuTimes *out) {
    if (strncmp(line, "cpu", 3) != 0) return 0;

    size_t i = 0;
    while (line[i] && line[i] != ' ' && i + 1 < labsz) {
        out_label[i] = line[i];
        i++;
    }
    out_label[i] = '\0';

    const char *p = line;
    while (*p && (*p < '0' || *p > '9')) p++;

    unsigned long long v[10] = {0};
    int n = 0;
    while (*p && n < 10) {
        char *end = NULL;
        unsigned long long x = strtoull(p, &end, 10);
        if (end == p) break;
        v[n++] = x;
        p = end;
        while (*p == ' ' || *p == '\t') p++;
    }

    out->nfields = n;
    for (int k = 0; k < 10; k++)
        out->field[k] = (k < n ? v[k] : 0ULL);
    return 1;
}

static int read_proc_stat(CpuSet *out_set) {
    FILE *f = fopen("/proc/stat", "r");
    if (!f) {
        perror("fopen /proc/stat");
        return -1;
    }

    cpuset_free(out_set);
    cpuset_init(out_set);

    char *line = NULL;
    size_t len = 0;
    while (getline(&line, &len, f) != -1) {
        if (strncmp(line, "cpu", 3) != 0) continue;
        char label[32];
        CpuTimes t;
        if (parse_cpu_line(line, label, sizeof(label), &t)) {
            cpuset_push(out_set, label, t);
        }
    }
    free(line);
    fclose(f);
    return (int)out_set->count;
}

static double calculate_cpu_usage(const CpuTimes *prev, const CpuTimes *cur) {
    unsigned long long prev_idle = prev->field[3] + prev->field[4];
    unsigned long long idle = cur->field[3] + cur->field[4];

    unsigned long long prev_non_idle = prev->field[0] + prev->field[1] + prev->field[2]
                                     + prev->field[5] + prev->field[6] + prev->field[7];
    unsigned long long non_idle = cur->field[0] + cur->field[1] + cur->field[2]
                                + cur->field[5] + cur->field[6] + cur->field[7];

    unsigned long long prev_total = prev_idle + prev_non_idle;
    unsigned long long total = idle + non_idle;

    unsigned long long total_delta = (total >= prev_total) ? total - prev_total : 0;
    unsigned long long idle_delta = (idle >= prev_idle) ? idle - prev_idle : 0;

    if (total_delta == 0) return 0.0;
    return (double)(total_delta - idle_delta) * 100.0 / (double)total_delta;
}

void cpu_monitor_init(void) {
    pthread_mutex_lock(&data_mutex);
    if (read_proc_stat(&prev_stats) > 0) {
        core_count = (prev_stats.count > 0) ? prev_stats.count - 1 : 0;
        printf("CPU Monitor: обнаружено %zu ядер\n", core_count);
    }
    pthread_mutex_unlock(&data_mutex);
}

void cpu_monitor_cleanup(void) {
    pthread_mutex_lock(&data_mutex);
    cpuset_free(&prev_stats);
    pthread_mutex_unlock(&data_mutex);
}

int cpu_monitor_get_usage(CpuUsageData *data) {
    CpuSet current = {NULL, 0, 0}; // FIX

    pthread_mutex_lock(&data_mutex);

    if (read_proc_stat(&current) <= 0) {
        pthread_mutex_unlock(&data_mutex);
        return -1;
    }

    // Инициализируем структуру данных
    data->total_usage = 0.0;
    data->core_count = (current.count > 0) ? current.count - 1 : 0;

    if (data->core_count > 0) {
        data->core_usage = (double*)malloc(data->core_count * sizeof(double)); // FIX
        if (!data->core_usage) {
            pthread_mutex_unlock(&data_mutex);
            return -1;
        }
    } else {
        data->core_usage = NULL;
    }

    // Рассчитываем общую загрузку
    if (prev_stats.count > 0 && current.count > 0) {
        data->total_usage = calculate_cpu_usage(&prev_stats.arr[0].times,
                                               &current.arr[0].times);

        // Рассчитываем поядерную загрузку
        size_t min_count = (prev_stats.count < current.count) ? prev_stats.count : current.count;
        for (size_t i = 1; i < min_count && i - 1 < data->core_count; i++) {
            data->core_usage[i - 1] = calculate_cpu_usage(&prev_stats.arr[i].times,
                                                         &current.arr[i].times);
        }
    }

    // Обновляем предыдущие данные
    cpuset_free(&prev_stats);
    prev_stats = current;

    pthread_mutex_unlock(&data_mutex);
    return 0;
}

size_t cpu_monitor_get_core_count(void) {
    return core_count;
}

// UDP клиент функции
static void* udp_client_thread(void* arg) {
    (void)arg; // FIX: убираем warning unused parameter

    struct sockaddr_in server_addr;

    udp_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (udp_socket < 0) {
        perror("socket creation failed");
        return NULL;
    }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(1234);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

    printf("UDP Client: запущен, отправка данных на localhost:1234\n");

    while (udp_running) {
        CpuUsageData data;
        if (cpu_monitor_get_usage(&data) == 0) {
            // Формируем строку с данными
            char buffer[1024];
            int len = snprintf(buffer, sizeof(buffer), "%.2f", data.total_usage);

            // Добавляем данные по ядрам
            for (size_t i = 0; i < data.core_count && (size_t)len < sizeof(buffer) - 10; i++) {
                len += snprintf(buffer + len, sizeof(buffer) - len, ",%.2f", data.core_usage[i]);
            }

            sendto(udp_socket, buffer, len, 0,
                   (const struct sockaddr*)&server_addr, sizeof(server_addr));

            free(data.core_usage);
        }

        // Задержка 1 Гц
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
    }

    if (udp_socket >= 0) {
        close(udp_socket);
        udp_socket = -1;
    }

    printf("UDP Client: остановлен\n");
    return NULL;
}

void cpu_udp_client_start(void) {
    if (udp_running) return;

    udp_running = 1;
    if (pthread_create(&udp_thread, NULL, udp_client_thread, NULL) != 0) {
        perror("pthread_create failed");
        udp_running = 0;
    }
}

void cpu_udp_client_stop(void) {
    if (!udp_running) return;

    udp_running = 0;
    pthread_join(udp_thread, NULL);
}

int cpu_udp_client_is_running(void) {
    return udp_running;
}
