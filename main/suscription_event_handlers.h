#ifndef SUSCRIPTION_EVENT_HANDLERS_H
#define SUSCRIPTION_EVENT_HANDLERS_H

#include "mqtt_queue.h"
#include "cJSON.h"
#include "esp_log.h"
#include "mqtt_utils.h"
#include "mqtt_queue.h"

/* suscriber_config_handler
*  Description: Event handler for the config global suscription
*/
void suscriber_global_config_handler(char* topic, char* message);
void suscriber_particular_config_handler(char* topic, char* message);

char * new_config_message(char* action, char* type, cJSON* payload);
#endif // SUSCRIPTION_EVENT_HANDLERS_H