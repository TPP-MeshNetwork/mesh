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

extern char *clientIdentifier;
extern mqtt_queues_t *mqtt_queues;
extern char * FIRMWARE_VERSION;

void add_relay(char *name, int pin);
void relay_init();
cJSON* get_relay_state();
int set_relay_state(int relay_id, int state);

// struct related to onTime Feature for relays
typedef struct onTimeTaskArgs {
  int relay_id;
  int onTime;
} onTimeTaskArgs_t;
void onTimeTask(void *args);
int get_on_time_status(int relay_id);
void set_on_time_status(int relay_id, size_t onTime);
void release_on_time_status(int relay_id);

#endif // RELAYS_H
