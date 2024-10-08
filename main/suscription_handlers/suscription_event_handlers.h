#ifndef SUSCRIPTION_EVENT_HANDLERS_H
#define SUSCRIPTION_EVENT_HANDLERS_H

#include "mqtt_queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt/utils/mqtt_utils.h"
#include "mqtt_queue.h"
#include "tasks_config.h"
#include "../mqtt/utils/mqtt_utils.h"

/* suscriber_config_handler
*  Description: Event handler for the config global suscription
*/
void suscriber_global_config_handler(char* topic, char* message);
void suscriber_particular_config_handler(char* topic, char* message);

/* relay_event_handler
*  Description: Event handler for the relay suscription
*/
void relay_event_handler(char* topic, char* message);
void relay_init();

typedef struct {
    char *topic;
    char *message;
    void (*handler)(char*, char*);
} suscription_event_handler_t;

extern const char FIRMWARE_VERSION[];
extern const char FIRMWARE_REVISION[];

#endif // SUSCRIPTION_EVENT_HANDLERS_H