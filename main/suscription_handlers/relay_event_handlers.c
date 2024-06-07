/* 
*   This file contains the event handlers for the relay suscriptions
*   Topic: /mesh/[mesh_id]/devices/[device_id]/relay
*   Publishes on the topic: /mesh/[mesh_id]/devices/[device_id]/relay/dashboard
*/
#include "suscription_event_handlers.h"
#include "driver/gpio.h"

// PIN GPIO22
#define RELAY1_PIN GPIO_NUM_22
// PIN GPIO23
#define RELAY2_PIN GPIO_NUM_23

extern char *clientIdentifier;
extern mqtt_queues_t mqtt_queues;

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
  * Function: relay_event_handler
  * ----------------------------
  *   Event handler for the relay suscription
  *
*/
void relay_event_handler(char* topic, char* message) {
    ESP_LOGI("[relay_event_handler]", "Relay event handler");
    ESP_LOGI("[relay_event_handler]", "Message: %s", message);
    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        ESP_LOGE("[relay_event_handler]", "Error parsing JSON");
        char *msg_payload;
        asprintf(&msg_payload, "\"status\": \"ok\", \"message\": \"Error parsing JSON\"");
        asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, msg_payload);
        publish(create_topic("relay", "dashboard", false), message);
        free(msg_payload);
        return;
    }

    // get the action of the message
    char* action = cJSON_GetObjectItem(root,"action")->valuestring;
    // get the type
    char* type = cJSON_GetObjectItem(root,"type")->valuestring;
    // get the payload
    cJSON *payload = cJSON_GetObjectItem(root,"payload");

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
            cJSON_AddNumberToObject(relay1, "state", gpio_get_level(RELAY1_PIN));
            cJSON_AddItemToArray(relays, relay1);

            cJSON *relay2 = cJSON_CreateObject();
            cJSON_AddNumberToObject(relay2, "relay_id", 2);
            cJSON_AddNumberToObject(relay2, "state", gpio_get_level(RELAY2_PIN));
            cJSON_AddItemToArray(relays, relay2);

            cJSON_AddItemToObject(payloadObj, "payload", relays);            

            char *payload_str = cJSON_Print(payloadObj);
            cJSON_Delete(payloadObj);

            // Create the topic
            char *topic = create_topic("relay", "dashboard", false);

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
            // {"action":"write","sender_client_id":"iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b","type":"relay","payload": [{"relay_id": 1, "state": 1}]}
            
            // Creating the response payload object
            cJSON *payloadResObj = cJSON_CreateObject();
            cJSON_AddStringToObject(payloadResObj, "action", "write");
            cJSON_AddStringToObject(payloadResObj, "sender_client_id", clientIdentifier);
            cJSON_AddStringToObject(payloadResObj, "type", "relay");

            // create array for payload
            cJSON *payloadArray = cJSON_CreateArray();
            
            int relay_len = cJSON_GetArraySize(payload);
            // for each element of the array of relays
            for (size_t i = 0; i< relay_len; i++) {
                // get the relay item from the array
                cJSON *payloadItem = cJSON_GetArrayItem(payload, i);
                // Getting the relay_id and state
                int relay_id = cJSON_GetObjectItem(payloadItem, "relay_id")->valueint;
                int state = cJSON_GetObjectItem(payloadItem, "state")->valueint;

                cJSON *itemRelay = cJSON_CreateObject();
                cJSON_AddNumberToObject(itemRelay, "relay_id", relay_id);
                
                // Check values of state
                if ( state != 0 && state != 1) {
                    cJSON_AddStringToObject(itemRelay, "status", "error");
                    cJSON_AddStringToObject(itemRelay, "message", "Invalid state value");
                    cJSON_AddItemToArray(payloadArray, itemRelay);
                    continue;
                }

                cJSON_AddNumberToObject(itemRelay, "state", state);
                switch(relay_id) {
                    case 1:
                        gpio_set_level(RELAY1_PIN, state);
                        cJSON_AddStringToObject(itemRelay, "status", "ok");
                        break;
                    case 2:
                        gpio_set_level(RELAY2_PIN, state);
                        cJSON_AddStringToObject(itemRelay, "status", "ok");
                        break;
                    default:
                        cJSON_AddStringToObject(itemRelay, "status", "error");
                        cJSON_AddStringToObject(itemRelay, "message", "Invalid relay_id");
                        break;
                }
                cJSON_AddItemToArray(payloadArray, itemRelay);
            }

            // add payloadResp to the response object
            cJSON_AddItemToObject(payloadResObj, "payload", payloadArray);
            
            char *payload_str = cJSON_Print(payloadResObj);
            cJSON_Delete(payloadResObj);

            // Create the topic
            char *topic = create_topic("relay", "dashboard", false);

            // Publish the message
            publish(topic, payload_str);
            free(topic);
            free(payload_str);
        }
    }
}