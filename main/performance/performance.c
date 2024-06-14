#include "performance.h"

uint64_t uptime = 0;

void log_memory() {
    uint32_t free_heap_size = 0, min_free_heap_size = 0;
    free_heap_size = esp_get_free_heap_size();
    min_free_heap_size = esp_get_minimum_free_heap_size();
    ESP_LOGI("log_perfdata", "\n\n free heap size = %ld \t  min_free_heap_size = %ld \n\n", free_heap_size, min_free_heap_size);
}


void set_uptime() {
    time_t now;
    time(&now);

    uptime = now;
}

uint64_t get_uptime() {
    return uptime;
}

cJSON* get_memory_stats() {
    uint32_t free_heap_size = 0, min_free_heap_size = 0;
    free_heap_size = esp_get_free_heap_size();
    min_free_heap_size = esp_get_minimum_free_heap_size();
    cJSON *memory_stats = cJSON_CreateObject();
    cJSON_AddNumberToObject(memory_stats, "free_heap_size", free_heap_size);
    cJSON_AddNumberToObject(memory_stats, "min_free_heap_size", min_free_heap_size);
    return memory_stats;
}