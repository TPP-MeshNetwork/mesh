#include "performance.h"

void log_memory() {
    uint32_t free_heap_size = 0, min_free_heap_size = 0;
    free_heap_size = esp_get_free_heap_size();
    min_free_heap_size = esp_get_minimum_free_heap_size();
    ESP_LOGI("log_perfdata", "\n\n free heap size = %ld \t  min_free_heap_size = %ld \n\n", free_heap_size, min_free_heap_size);
}