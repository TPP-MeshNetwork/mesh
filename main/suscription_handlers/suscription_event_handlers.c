/* This file contains the event handlers for the suscriptions
*/
#include "suscription_event_handlers.h"

extern char *clientIdentifier;
extern mqtt_queues_t mqtt_queues;

/* suscriber_particular_config_handler
*  Description: Event handler for the config particular suscription
*/
void suscriber_particular_config_handler(char* topic, char* message) {
    ESP_LOGI("[suscriber_particular_config_handler]", "Config EVENT HANDLER");
    ESP_LOGI("[suscriber_particular_config_handler]", "Message: %s", message);
    cJSON *root = cJSON_Parse(message);
    if (root == NULL) {
        ESP_LOGE("[suscriber_particular_config_handler]", "Error parsing JSON");
        return;
    }

    // get the sender_client_id from the message
    char* sender_client_id = cJSON_GetObjectItem(root,"sender_client_id")->valuestring;
    // just to ignore if we are not doing the correct stuff
    // if the sender_client_id is the same as the current client_id then ignore the message
    if (strcmp(sender_client_id, clientIdentifier) == 0) {
        ESP_LOGI("[suscriber_particular_config_handler]", "Ignoring message from self");
        return;
    }

    // get the action of the message
    char* action = cJSON_GetObjectItem(root,"action")->valuestring;
    // get the type
    char* type = cJSON_GetObjectItem(root,"type")->valuestring;
    // get the payload
    cJSON *payload = cJSON_GetObjectItem(root,"payload");

    char *to_send_message = new_config_message(action, type, payload);

    if(to_send_message != NULL)
        publish(create_topic("config", "dashboard", true), to_send_message);

    free(to_send_message);
}


/* suscriber_global_config_handler
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

    char *to_send_message = new_config_message(action, type, payload);

    if(to_send_message != NULL)
        publish(create_topic("config", "dashboard", true), to_send_message);

    free(to_send_message);
}


char * read_sensor_config() {
    char *payload;
    asprintf(&payload, "\"sensors\": [{\"type\": \"temperature\", \"pool\": {\"time\": 5, \"max\": 10, \"min\": 1}, \"active\": true}, {\"type\": \"humidity\", \"pool\": {\"time\": 10, \"max\": 10, \"min\": 1}, \"active\": true}]");
    return payload;
}

char * new_config_message(char* action, char* type, cJSON *payload) {
    char * message = NULL;
    if (!strcmp(type, "config")) {
        if (!strcmp(action, "read")) {
            // Read the configuration
            // Example json to send: 
            // {
            // "action": "read", // "write"
            //     "sender_client_id": <client_id_esp>, 
            //     "type": "config" 
            //     "payload": { 
            //         "sensors": [{
            //             "type": "temperature",
            //             "pool": { time: 5, max: 10, min: 1 },
            //             "active": true,
            //         },
            //         {
            //             "type": "humidity",
            //             "pool": { time: 5, max: 10, min: 1 },
            //             "active": true
            //         }]
            //     }
            // }
            char *payload = read_sensor_config();
            asprintf(&message, "{\"action\": \"read\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, payload);

        } else if (!strcmp(action, "write")) {
            // write the configuration
            // publish the acknoleadge of the configuration
            // Read the configuration
            // Example ack: 
            // {
            // "action": "write"
            //     "sender_client_id": <client_id_esp>, 
            //     "type": "config" 
            //     "payload": { 
            //         "status": "ok", // "error"
            //         "message": "Configuration saved" // "Error saving configuration: sensor temperature: (max out of range)"
            //     }
            // }

            char *payload;
            asprintf(&payload, "\"status\": \"ok\", \"message\": \"Configuration saved\"");
            asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, payload);
        } else {
            ESP_LOGE("[new_config_message]", "Unknown action");
        }
    } else {
        ESP_LOGE("[new_config_message]", "Unknown type");
    }
    return message;
}
