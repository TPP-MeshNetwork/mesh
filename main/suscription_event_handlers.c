/* This file contains the event handlers for the suscriptions
*/
#include "suscription_event_handlers.h"

extern char *clientIdentifier;
extern mqtt_queues_t mqtt_queues;

/* suscriber_config_handler
*  Description: Event handler for the config global suscription
*/
void suscriber_global_config_handler(char* topic, char* message) {
    ESP_LOGI("[suscriber_config_handler]", "Config EVENT HANDLER");
    ESP_LOGI("[suscriber_config_handler]", "Message: %s", message);
    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        ESP_LOGE("[suscriber_config_handler]", "Error parsing JSON");
        return;
    }

    // get the sender_client_id from the message
    char* sender_client_id = cJSON_GetObjectItem(root,"sender_client_id")->valuestring;
    // if the sender_client_id is the same as the current client_id then ignore the message
    if (strcmp(sender_client_id, clientIdentifier) == 0) {
        ESP_LOGI("[suscriber_config_handler]", "Ignoring message from self");
        return;
    }
    // get the action of the message
    char* action = cJSON_GetObjectItem(root,"action")->valuestring;
    // get the type
    char* type = cJSON_GetObjectItem(root,"type")->valuestring;
    // get the payload
    cJSON *payload = cJSON_GetObjectItem(root,"payload");

    if (!strcmp(type, "config")) {
        if (!strcmp(action, "read")) {
            // read the configuration

            // {
            // "action": "read", // "write"
            //     "sender_client_id": <client_id_esp>, 
            //     "type": "config" 
            //     "payload": { 
            //         "sensors": [{
            //             "type": "temperature",
            //             "pool_time": 5,
            //             "active": true,
            //         },
            //         {
            //             "type": "humidity",
            //             "pool_time": 10,
            //             "active": true
            //         }]
            //     }
            // }

            char *payload;
            asprintf(&payload, "\"sensors\": [{\"type\": \"temperature\", \"pool_time\": 5, \"active\": true}, {\"type\": \"humidity\", \"pool_time\": 10, \"active\": true}]");
            char *message;
            asprintf(&message, "{\"action\": \"read\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, payload);
            publish(create_topic("config", "", false), message);
        } else if (!strcmp(action, "write")) {
            // write the configuration
            // publish the configuration
        } else {
            ESP_LOGE("[suscriber_config_handler]", "Unknown action");
        }
    } else {
        ESP_LOGE("[suscriber_config_handler]", "Unknown type");
    }

}