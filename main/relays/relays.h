#ifndef RELAYS_H
#define RELAYS_H

#include "esp_log.h"
#include "driver/gpio.h"
#include "cJSON.h"
#include "mqtt_queue.h"
#include "mqtt/utils/mqtt_utils.h"

// PIN GPIO22
#define RELAY1_PIN GPIO_NUM_22
// PIN GPIO23
#define RELAY2_PIN GPIO_NUM_23

#define RELAYS_LEN 2

extern char *clientIdentifier;
extern mqtt_queues_t *mqtt_queues;
extern char * FIRMWARE_VERSION;


void relay_init();
cJSON* get_relay_state();

#endif // RELAYS_H
