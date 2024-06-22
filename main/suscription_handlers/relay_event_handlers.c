/* 
*   This file contains the event handlers for the relay suscriptions
*   Topic: /mesh/[mesh_id]/devices/[device_id]/relay
*   Publishes on the topic: /mesh/[mesh_id]/relay/dashboard
*/
#include "../relays/relays.h"
#include "../performance/performance.h"

char * create_message_relay(char* type, cJSON* payload) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "action", "relay");
    cJSON_AddStringToObject(root, "sender_client_id", clientIdentifier);
    cJSON_AddStringToObject(root, "type", type);
    cJSON *firmware = cJSON_CreateObject();
    cJSON_AddStringToObject(firmware, "version", FIRMWARE_VERSION);
    cJSON_AddItemToObject(root, "firmware", firmware);

    if (payload != NULL)
        cJSON_AddItemToObject(root, "payload", payload);
    
    char *message = cJSON_PrintUnformatted(root);
    cJSON_Delete(root);
    return message;
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

            // if read has payload send error
            if (payload != NULL) {
                cJSON *payload_resp = cJSON_CreateObject();
                cJSON *payload_array = cJSON_CreateArray();
                cJSON *item_relay = cJSON_CreateObject();
                cJSON_AddStringToObject(item_relay, "status", "error");
                cJSON_AddStringToObject(item_relay, "message", "Invalid payload");
                cJSON_AddItemToArray(payload_array, item_relay);
                cJSON_AddItemToObject(payload_resp, "relay", payload_array);
                char * msg_read = create_message_relay("read", payload_resp);
                message = create_mqtt_message(msg_read);
                free(msg_read);
                char *topic = create_topic("relay", "dashboard", false);
                publish(topic, message);
                free(topic);
                free(message);
                cJSON_Delete(root);
                return;
            }

            // Create the payload object
            cJSON *payload_resp = cJSON_CreateObject();
            cJSON *payload_array = get_relay_state();
             // add relay json inside payloadResp
            cJSON_AddItemToObject(payload_resp, "relay", payload_array);
            
            char * msg_read = create_message_relay("read", payload_resp);
            message = create_mqtt_message(msg_read);
            free(msg_read);

            // Create the topic
            char *topic = create_topic("relay", "dashboard", false);

            // Publish the message
            publish(topic, message);
            free(topic);
            free(message);
        } else if (!strcmp(action, "write")) {
            // Write the relay configuration
            // Example:
            // {
            //     "action": "write",
            //     "sender_client_id": "iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b", 
            //     "type": "relay",
            //     "payload": { 
            //       "relay": [{
            //         "relay_id": 1,
            //         "state": 1
            //       }],
            //    }
            // }
            // Minified Example:
            // {"action":"write","sender_client_id":"iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b","type":"relay","payload":{relay:[{"id": 1,"state":1}]}}

            // create array for payload
            cJSON *payload_resp = cJSON_CreateObject();
            cJSON *payload_array = cJSON_CreateArray();

            // get the relay array from the payload if exists
            if (!cJSON_HasObjectItem(payload, "relay")) {
                cJSON *item_relay = cJSON_CreateObject();
                cJSON_AddStringToObject(item_relay, "status", "error");
                cJSON_AddStringToObject(item_relay, "message", "Invalid payload");
                cJSON_AddItemToArray(payload_array, item_relay);
                cJSON_AddItemToObject(payload_resp, "relay", payload_array);
                char * msg_read = create_message_relay("write", payload_resp);
                message = create_mqtt_message(msg_read);
                free(msg_read);
                char *topic = create_topic("relay", "dashboard", false);
                publish(topic, message);
                free(topic);
                free(message);
                cJSON_Delete(root);
                return;
            }
                        
            cJSON* payload_relay = cJSON_GetObjectItem(payload, "relay");
            int relay_len = cJSON_GetArraySize(payload_relay);
            // for each element of the array of relays
            for (size_t i = 0; i< relay_len; i++) {
                // get the relay item from the array
                cJSON *payload_item = cJSON_GetArrayItem(payload_relay, i);
                // Getting the relay_id and state
                int relay_id = cJSON_GetObjectItem(payload_item, "id")->valueint;

                int state = -1;
                if (cJSON_HasObjectItem(payload_item, "state")) {
                    state = cJSON_GetObjectItem(payload_item, "state")->valueint;
                }

                // if onTime is present in the payload_item read it
                int onTime = -1;
                if (cJSON_HasObjectItem(payload_item, "onTime")) {
                    onTime = cJSON_GetObjectItem(payload_item, "onTime")->valueint;
                }

                cJSON *item_relay = cJSON_CreateObject();
                cJSON_AddNumberToObject(item_relay, "id", relay_id);
                
                // Check values of state if onTime is not present
                if ( onTime == -1 && state != 0 && state != 1) {
                    cJSON_AddStringToObject(item_relay, "status", "error");
                    cJSON_AddStringToObject(item_relay, "message", "Invalid state value");
                    cJSON_AddItemToArray(payload_array, item_relay);
                    continue;
                }

                cJSON_AddNumberToObject(item_relay, "state", state);

                if (onTime > 0) {
                    // if relay->onTimeTaskActive is 1 then return and do not turn off the relay
                    int onTimeStatus = get_on_time_status(relay_id);
                    if (onTimeStatus == 1) {
                        ESP_LOGI("[onTimeTask]", "Relay id: %d already has an onTimeTask active", relay_id);
                        cJSON_AddStringToObject(item_relay, "status", "error");
                        cJSON_AddStringToObject(item_relay, "message", "Relay already has an onTimeTask active");
                    } else if (onTimeStatus == -1) {
                        ESP_LOGI("[onTimeTask]", "Relay id: %d not found", relay_id);
                        cJSON_AddStringToObject(item_relay, "status", "error");
                        cJSON_AddStringToObject(item_relay, "message", "Relay not found");
                    } else {
                        // Create the task to swith off the relay
                        onTimeTaskArgs_t *args = malloc(sizeof(struct onTimeTaskArgs));
                        args->relay_id = relay_id;
                        args->onTime = onTime;
                        xTaskCreate(onTimeTask, "onTimeTask", 2048, (void *) args, 5, NULL);
                        log_memory();
                    }
                } else {
                    // Set the relay state
                    int status = set_relay_state(relay_id, state);
                    if (status == -1) {
                        cJSON_AddStringToObject(item_relay, "status", "error");
                        cJSON_AddStringToObject(item_relay, "message", "Invalid relay_id");
                        continue;
                    } else if (status == -2) {
                        cJSON_AddStringToObject(item_relay, "status", "error");
                        cJSON_AddStringToObject(item_relay, "message", "Invalid state value");
                        continue;
                    } else {
                        cJSON_AddStringToObject(item_relay, "status", "ok");
                    }
                }
                cJSON_AddItemToArray(payload_array, item_relay);
            }

            // add relay json inside payloadResp
            cJSON_AddItemToObject(payload_resp, "relay", payload_array);
            
            char * msg_read = create_message_relay("write", payload_resp);
            message = create_mqtt_message(msg_read);
            free(msg_read);

            // Create the topic
            char *topic = create_topic("relay", "dashboard", false);

            // Publish the message
            publish(topic, message);
            free(topic);
            free(message);
        }
    }
    cJSON_Delete(root);
}