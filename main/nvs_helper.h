#include "nvs_flash.h"
#include "esp_log.h"

char * nvs_load_value_if_exist(const char* tag, nvs_handle_t *handle, const char* key);
