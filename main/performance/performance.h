#ifndef PERFOMANCE_H
#define PERFOMANCE_H
#include "esp_system.h"
#include "esp_log.h"
#include "cJSON.h"
#include <time.h>

void log_memory(void);
void set_uptime(void);
uint64_t get_uptime(void);
cJSON* get_memory_stats(void);

#endif // PERFOMANCE_H