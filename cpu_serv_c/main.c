#include "cpu_monitor.h"
#include <stdio.h>
#include <signal.h>
#include <string.h>

static volatile int running = 1;

void signal_handler(int sig) {
    running = 0;
}

int main(int argc, char *argv[]) {
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    cpu_monitor_init();
    
    printf("CPU Monitor Demo\n");
    printf("Обнаружено ядер: %zu\n", cpu_monitor_get_core_count());
    printf("Опрос: 1 Гц. Нажмите Ctrl+C для выхода.\n\n");
    
    // Проверяем аргументы командной строки
    int udp_mode = 0;
    if (argc > 1 && strcmp(argv[1], "--udp") == 0) {
        udp_mode = 1;
        cpu_udp_client_start();
        printf("Режим UDP: данные отправляются на localhost:1234\n");
    }
    
    while (running) {
        if (!udp_mode) {
            // Локальный вывод
            CpuUsageData data;
            if (cpu_monitor_get_usage(&data) == 0) {
                printf("Total: %6.2f%%", data.total_usage);
                
                for (size_t i = 0; i < data.core_count; i++) {
                    printf("  CPU%zu: %6.2f%%", i, data.core_usage[i]);
                }
                printf("\n");
                
                free(data.core_usage);
            }
        }
        
        // Задержка 1 Гц
        struct timespec ts = {1, 0};
        nanosleep(&ts, NULL);
    }
    
    if (udp_mode) {
        cpu_udp_client_stop();
    }
    
    cpu_monitor_cleanup();
    printf("\nПрограмма завершена.\n");
    
    return 0;
}

// gcc -std=c11 -O2 -Wall -Wextra -pthread -c cpu_monitor.c -o cpu_monitor.o
// gcc -std=c11 -O2 -Wall -Wextra -pthread main.c cpu_monitor.o -o cpu_monitor

// # Локальный режим
// ./cpu_monitor

// # UDP режим
// ./cpu_monitor --udp
