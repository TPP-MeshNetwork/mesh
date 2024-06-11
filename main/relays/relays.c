#include "relays.h"

int relays_pin[RELAYS_LEN] = {RELAY1_PIN, RELAY2_PIN};

/* 
  * Function: relay_init
  * ----------------------------
  *   Initialize the relay pins
  *
*/
void relay_init() {
    // Configure GPIO pin as output
    gpio_set_direction(RELAY1_PIN, GPIO_MODE_OUTPUT);

    // Set initial level to turn off the LED
    gpio_set_level(RELAY1_PIN, 0);

    // Disable pull-up/pull-down resistors
    gpio_set_pull_mode(RELAY1_PIN, GPIO_PULLUP_DISABLE);
    gpio_set_pull_mode(RELAY1_PIN, GPIO_PULLDOWN_DISABLE);

    // Configure GPIO pin as output
    gpio_set_direction(RELAY2_PIN, GPIO_MODE_OUTPUT);

    // Set initial level to turn off the LED
    gpio_set_level(RELAY2_PIN, 0);

    // Disable pull-up/pull-down resistors
    gpio_set_pull_mode(RELAY2_PIN, GPIO_PULLUP_DISABLE);
    gpio_set_pull_mode(RELAY2_PIN, GPIO_PULLDOWN_DISABLE);
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
    for(size_t i = 0; i < RELAYS_LEN; i++) {
        cJSON *relay = cJSON_CreateObject();
        cJSON_AddNumberToObject(relay, "relay_id", i+1);
        cJSON_AddNumberToObject(relay, "state", gpio_get_level(relays_pin[i]));
        cJSON_AddItemToArray(relays, relay);
    }
    return relays;
}