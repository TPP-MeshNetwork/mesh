/* 
*   This file contains the event handlers for the relay suscriptions
*   Topic: /mesh/[mesh_id]/devices/[device_id]/relay
*   Publishes on the topic: /mesh/[mesh_id]/devices/[device_id]/relay/dashboard
*/
#include "../relays/relays.h"


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

            cJSON *relays_array = get_relay_state();

            cJSON_AddItemToObject(payloadObj, "payload", relays_array);            

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