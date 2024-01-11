
#include "nvs_helper.h"

char * nvs_load_value_if_exist(const char *tag, nvs_handle *handle, const char* key) {
    // Try to get the size of the item
    size_t value_size;
    esp_err_t err = nvs_get_str(*handle, key, NULL, &value_size);
    printf("err: %d\n", err);
    if( err != ESP_OK) {
        ESP_LOGE(tag, "Failed to get size of key: %s", key);
        return NULL;
    }

    char *value = malloc(value_size);
    if(nvs_get_str(*handle, key, value, &value_size) != ESP_OK) {
        ESP_LOGE(tag, "Failed to load key: %s", key);
        free(value);
        return NULL;
    }

    return value;
}