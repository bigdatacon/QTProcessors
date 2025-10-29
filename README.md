## Видео по проекту
https://disk.yandex.ru/i/bRUBcm1zzdPfiA

## Запуск udp клиента в терминале в папке cpu_client:
### компиляция:
gcc -std=c11 -O2 -Wall -Wextra -pthread -c cpu_monitor.c -o cpu_monitor.o 

gcc -std=c11 -O2 -Wall -Wextra -pthread main.c cpu_monitor.o -o cpu_monitor


## Запуск:
### Локальный режим
./cpu_monitor

### UDP режим
./cpu_monitor --udp



## Запуск qt графика - в qt creator:
собрать ->запустить

В терминале:
## В папке с Qt проектом
qmake cpu_server.pro
make
./cpu_server
