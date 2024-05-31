#include "persistence.h"

persistence_err_t persistence_init(void) {
    esp_err_t err = nvs_flash_init();
    if (err == ESP_OK) {
        return INITIALIZED_PERSISTENCE;
    } else {
        return UNABLE_INITIALIZE_PERSISTENCE;
    }
}


persistence_err_t persistence_erase_all(void) {
    ESP_ERROR_CHECK(nvs_flash_erase());
    return PERSISTENCE_OP_OK;
}


persistence_err_t persistence_erase_namespace(void) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(PERSISTENCE_NAMESPACE, NVS_READWRITE, &nvs_handle));
    ESP_ERROR_CHECK(nvs_erase_all(nvs_handle));
    nvs_commit(nvs_handle);
    nvs_close(nvs_handle);
    return PERSISTENCE_OP_OK;
}


persistence_handler_t persistence_open(void) {
    nvs_handle_t nvs_handle;
    ESP_ERROR_CHECK(nvs_open(PERSISTENCE_NAMESPACE, NVS_READWRITE, &nvs_handle));
    return (persistence_handler_t) nvs_handle;
}


persistence_err_t persistence_close(persistence_handler_t handler) {
    nvs_handle_t nvs_handle = (nvs_handle_t) handler;
    nvs_close(nvs_handle);
    return PERSISTENCE_OP_OK;
}


persistence_err_t persistence_get_str(persistence_handler_t handler, const char *key, char **value) {
    nvs_handle_t nvs_handle = (nvs_handle_t) handler;

    size_t nvs_str_size;

    esp_err_t err = nvs_get_str(nvs_handle, key, NULL, &nvs_str_size);
    *value = (char*) malloc(nvs_str_size * sizeof(char));
    err = nvs_get_str(nvs_handle, key, *value, &nvs_str_size);

    return err == ESP_OK? PERSISTENCE_OP_OK : PERSISTENCE_OP_FAIL;
}


persistence_err_t persistence_set_str(persistence_handler_t handler, const char *key, const char *value) {
    nvs_handle_t nvs_handle = (nvs_handle_t) handler;
    esp_err_t err = nvs_set_str(nvs_handle, key, value);
    if (err == ESP_OK) {
        return PERSISTENCE_OP_OK;
    } else {
        return PERSISTENCE_OP_FAIL;
    }
}


persistence_err_t persistence_set_u8(persistence_handler_t handler, const char *key, uint8_t value) {
    nvs_handle_t nvs_handle = (nvs_handle_t) handler;
    esp_err_t err = nvs_set_u8(nvs_handle, key, value);
    if (err == ESP_OK) {
        return PERSISTENCE_OP_OK;
    } else {
        return PERSISTENCE_OP_FAIL;
    }
}


persistence_err_t persistence_get_u8(persistence_handler_t handler, const char *key, uint8_t *value) {
    nvs_handle_t nvs_handle = (nvs_handle_t) handler;
    esp_err_t err = nvs_get_u8(nvs_handle, key, value);
    if (err == ESP_OK) {
        return PERSISTENCE_OP_OK;
    } else {
        return PERSISTENCE_OP_FAIL;
    }
}
