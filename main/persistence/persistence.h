#include "esp_log.h"
#include "nvs_flash.h"

typedef int persistence_err_t;
typedef uint32_t persistence_handler_t;

#define PERSISTENCE_NAMESPACE "storage"

#define UNABLE_INITIALIZE_PERSISTENCE 0
#define INITIALIZED_PERSISTENCE 1
#define PERSISTENCE_OP_OK 2
#define PERSISTENCE_OP_FAIL 3

void greet(void);
persistence_err_t persistence_init(void);
persistence_err_t persistence_erase_all(void);
persistence_err_t persistence_erase_namespace(void);
persistence_handler_t persistence_open(void);
persistence_err_t persistence_close(persistence_handler_t handler);
persistence_err_t persistence_get_str(persistence_handler_t handler, const char *key, char **value);
persistence_err_t persistence_set_str(persistence_handler_t handler, const char *key, const char *value);
persistence_err_t persistence_set_u8(persistence_handler_t handler, const char *key, uint8_t value);
persistence_err_t persistence_get_u8(persistence_handler_t handler, const char *key, uint8_t *value);
