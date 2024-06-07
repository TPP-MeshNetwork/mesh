/* 
*   This file contains the event handlers for the relay suscriptions
*   Topic: /mesh/[mesh_id]/devices/[device_id]/relay
*   Publishes on the topic: /mesh/[mesh_id]/devices/[device_id]/dashboard/relay
*/
#include "suscription_event_handlers.h"

// GPIO pins
#define RELAY1_PIN 2
// GPIO22
#define RELAY2_PIN 4

extern char *clientIdentifier;
extern mqtt_queues_t mqtt_queues;


int read_pin(int pin) {
    return gpio_get_level(pin);
}

/*
  * Function: relay_event_handler
  * ----------------------------
  *   Event handler for the relay suscription
  *
*/
void relay_event_handler(char* action, char* type, char *payload) {
    ESP_LOGI(MESH_TAG, "Relay event handler");
    if (!strcmp(type, "relay")) {
        if (!strcmp(action, "read")) {
            // Read the relay configuration
            // Example:
            // {
            //     "action": "read",
            //     "sender_client_id": "iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b", 
            //     "type": "relay"
            // }
            // Minified Example:
            // {"action":"read","sender_client_id":"iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b","type":"relay"}

            // Create the payload object
            cJSON *payloadObj = cJSON_CreateObject();
            cJSON_AddStringToObject(payloadObj, "action", "read");
            cJSON_AddStringToObject(payloadObj, "sender_client_id", clientIdentifier);
            cJSON_AddStringToObject(payloadObj, "type", "relay");

            // create array of relay states
            cJSON *relays = cJSON_CreateArray();
            cJSON *relay1 = cJSON_CreateObject();
            cJSON_AddNumberToObject(relay1, "relay_id", 1);
            cJSON_AddNumberToObject(relay1, "state", read_pin(RELAY1_PIN));
            cJSON_AddItemToArray(relays, relay1);

            cJSON *relay2 = cJSON_CreateObject();
            cJSON_AddNumberToObject(relay2, "relay_id", 2);
            cJSON_AddNumberToObject(relay2, "state", read_pin(RELAY2_PIN));
            cJSON_AddItemToArray(relays, relay2);

            cJSON_AddItemToObject(payloadObj, "payload", relays);            

            char *payload_str = cJSON_Print(payloadObj);
            cJSON_Delete(payloadObj);

            // Create the topic
            char *topic = create_topic("relay", "dashboard", true);

            // Publish the message
            publish(topic, payload_str);
            free(topic);
            free(payload_str);
        } else if (!strcmp(action, "write")) {
            // Write the relay configuration
            // Example:
            // {
            //     "action": "write",
            //     "sender_client_id": "iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b", 
            //     "type": "relay",
            //     "payload": [{
            //         "relay_id": 1,
            //         "state": 1
            //     }]
            // }
            // Minified Example:
            // {"action":"write","sender_client_id":"iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b","type":"relay","payload":{"relay_id":1,"state":1}}

            cJSON *payloadObj = cJSON_Parse(payload);
            cJSON *payloadObjPayload = cJSON_GetObjectItem(payloadObj, "payload");

            // Getting the relay_id and state
            int relay_id = cJSON_GetObjectItem(payloadObjPayload, "relay_id")->valueint;
            int state = cJSON_GetObjectItem(payloadObjPayload, "state")->valueint;

            // Check values of state
            if (state != 0 && state != 1) {
                ESP_LOGE(MESH_TAG, "Invalid state value");
                free(payloadObj);
                return;
            }

            // Setting the value of the relay
            if (relay_id == 1) {
                gpio_set_level(RELAY1_PIN, state);
            } else if (relay_id == 2) {
                gpio_set_level(RELAY2_PIN, state);
            }

            // Creating the payload object
            cJSON *payloadObj = cJSON_CreateObject();
            cJSON_AddStringToObject(payloadObj, "action", "write");
            cJSON_AddStringToObject(payloadObj, "sender_client_id", clientIdentifier);
            cJSON_AddStringToObject(payloadObj, "type", "relay");

            cJSON *payloadObjPayload = cJSON_CreateObject();
            cJSON_AddNumberToObject(payloadObjPayload, "relay_id", relay_id);
            cJSON_AddNumberToObject(payloadObjPayload, "state", state);
            cJSON_AddItemToObject(payloadObj, "payload", payloadObjPayload);

            char *payload_str = cJSON_Print(payloadObj);
            cJSON_Delete(payloadObj);

            // Create the topic
            char *topic = create_topic("relay", "dashboard", true);

            // Publish the message
            publish(topic, payload_str);
            free(topic);
            free(payload_str);
        }
    }
}