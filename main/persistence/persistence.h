#ifndef PERSISTENCE_H
#define PERSISTENCE_H

#include "esp_log.h"
#include "nvs_flash.h"

typedef uint32_t persistence_handler_t;
typedef enum {
    UNABLE_INITIALIZE_PERSISTENCE = 0,
    INITIALIZED_PERSISTENCE = 1,
    PERSISTENCE_OP_OK = 2,
    PERSISTENCE_OP_FAIL = 3
} persistence_err_t;

persistence_err_t persistence_init(void);
persistence_err_t persistence_erase_all(void);
persistence_err_t persistence_erase_namespace(char *namespace);
persistence_handler_t persistence_open(char *namespace);
persistence_err_t persistence_close(persistence_handler_t handler);
persistence_err_t persistence_get_str(persistence_handler_t handler, const char *key, char **value);
persistence_err_t persistence_set_str(persistence_handler_t handler, const char *key, const char *value);
persistence_err_t persistence_set_u8(persistence_handler_t handler, const char *key, uint8_t value);
persistence_err_t persistence_get_u8(persistence_handler_t handler, const char *key, uint8_t *value);

#endif // PERSISTENCE_H