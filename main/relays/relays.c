#include "relays.h"


typedef struct relays {
    int id;
    char * name;
    int pin;
    int onTimeTaskActive; // 1 if the onTime task is active, 0 if the onTime task is not active
    size_t onTimeFinish; // epoch time when the onTime task should finish
} relays_t;

relays_t **relaysConfig;
size_t RELAYS_LEN = 0;

/* 
  * Function: add_relay
  * ----------------------------
  *  Add a relay to the configuration
  *
*/
void add_relay(char *name, int pin) {
    relays_t *new_relay = malloc(sizeof(relays_t));
    new_relay->name = name;
    new_relay->id = RELAYS_LEN + 1;
    new_relay->pin = pin;
    new_relay->onTimeTaskActive = 0;
    new_relay->onTimeFinish = 0;
    // relaysConfig is a null terminated array
    relaysConfig = realloc(relaysConfig, (RELAYS_LEN + 2) * sizeof(relays_t *));
    relaysConfig[RELAYS_LEN] = new_relay;
    relaysConfig[RELAYS_LEN + 1] = NULL;
    RELAYS_LEN++;
    ESP_LOGI("[add_relay]", "Relay %s added for pin: %d", name, pin);
}

/* 
  * Function: relay_init
  * ----------------------------
  *   Initialize the relay pins
  *
*/
void relay_init() {
    if (relaysConfig == NULL || RELAYS_LEN == 0) {
        ESP_LOGI("[relay_init]", "No relays configured");
        return;
    }
    for(size_t i = 0; relaysConfig[i] != NULL; i++) {
        int pin = relaysConfig[i]->pin;
        // Configure GPIO pin as output
        gpio_set_direction(pin, GPIO_MODE_OUTPUT);

        // Set initial level to turn off the LED
        gpio_set_level(pin, 0);

        // Disable pull-up/pull-down resistors
        gpio_set_pull_mode(pin, GPIO_PULLUP_DISABLE);
        gpio_set_pull_mode(pin, GPIO_PULLDOWN_DISABLE);
    }
}

/*
  * Function: get_relay_state
  * ----------------------------
  *   Get the state of the relays
  *
  *   returns: cJSON object with the state of the relays
*/
cJSON* get_relay_state() {
    cJSON *relays = cJSON_CreateArray();
    for(size_t i = 0; relaysConfig[i] != NULL; i++) {
        cJSON *relay = cJSON_CreateObject();
        cJSON_AddNumberToObject(relay, "id", relaysConfig[i]->id);
        cJSON_AddStringToObject(relay, "name", relaysConfig[i]->name);
        cJSON_AddNumberToObject(relay, "state", gpio_get_level(relaysConfig[i]->pin));
        cJSON_AddNumberToObject(relay, "onTimeActive", relaysConfig[i]->onTimeTaskActive);
        cJSON_AddNumberToObject(relay, "onTimeFinish", relaysConfig[i]->onTimeFinish);
        cJSON_AddItemToArray(relays, relay);
    }
    return relays;
}

/*
  * Function: set_relay_state
  * ----------------------------
  *   Set the state of the relays
  *
  *   returns: cJSON object with the state of the relays
*/
int set_relay_state(int relay_id, int state) {
    
    // get relay id from relaysConfig
    int relay_index = -1;
    for(size_t i = 0; relaysConfig[i] != NULL; i++) {
        if (relaysConfig[i]->id == relay_id) {
            relay_index = i;
            break;
        }
    }
    if (relay_index == -1) {
        return -1;
    }
    if ( state != 0 && state != 1) {
        return -2;
    }

    gpio_set_level(relaysConfig[relay_index]->pin, state);
    return 0;
}

/*
  * Function: get_on_time_status
  * ----------------------------
  *   Get the status of the onTime task for a relay
  *
  *   returns: 1 if the task is active, 0 if the task is not active, -1 if the relay is not found
*/
int get_on_time_status(int relay_id) {
    for (size_t i = 0; relaysConfig[i] != NULL; i++) {
        if (relaysConfig[i]->id == relay_id) {
            return relaysConfig[i]->onTimeTaskActive;
        }
    }
    return -1;
}

/*
  * Function: set_on_time_status
  * ----------------------------
  *   Set the status of the onTime task for a relay
  *   The onTime task will finish after onTime milliseconds
  *
*/
void set_on_time_status(int relay_id, size_t onTime) {
    for (size_t i = 0; relaysConfig[i] != NULL; i++) {
        if (relaysConfig[i]->id == relay_id) {
            relaysConfig[i]->onTimeTaskActive = 1;
            time_t now;
            time(&now);
            relaysConfig[i]->onTimeFinish = now + onTime/1000;
            return;
        }
    }
}

/*
  * Function: release_on_time_status
  * ----------------------------
  *   Release the status of the onTime task for a relay
  *
*/
void release_on_time_status(int relay_id) {
    for (size_t i = 0; relaysConfig[i] != NULL; i++) {
        if (relaysConfig[i]->id == relay_id) {
            relaysConfig[i]->onTimeTaskActive = 0;
            relaysConfig[i]->onTimeFinish = 0;
            return;
        }
    }
}

/*
  * Function: onTimeTask
  * ----------------------------
  *   Task to turn off the relay after a certain time
  *
*/
void onTimeTask(void *args) {
    struct onTimeTaskArgs *task_args = (struct onTimeTaskArgs *) args;
    set_relay_state(task_args->relay_id, 1);
    set_on_time_status(task_args->relay_id, task_args->onTime);
    vTaskDelay(task_args->onTime / portTICK_PERIOD_MS);
    set_relay_state(task_args->relay_id, 0);
    release_on_time_status(task_args->relay_id);
    ESP_LOGI("[onTimeTask]", "Relay id: %d turned off after %d milliseconds", task_args->relay_id, task_args->onTime);
    free(task_args);
    vTaskDelete(NULL);
}