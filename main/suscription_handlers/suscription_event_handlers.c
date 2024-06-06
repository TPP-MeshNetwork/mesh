/* This file contains the event handlers for the suscriptions
*/
#include "suscription_event_handlers.h"

extern char *clientIdentifier;
extern mqtt_queues_t mqtt_queues;


char * create_read_sensor_response_json(Config_t *config[]) {
    // Create the payload object
    cJSON *payload = cJSON_CreateObject();
    cJSON *sensors_array = cJSON_CreateArray();

    // Populate sensors array
    for (size_t i = 0; config[i] != NULL; i++) {
        cJSON *sensor_object = cJSON_CreateObject();

        cJSON_AddNumberToObject(sensor_object, "task_id", config[i]->task_id);

        cJSON *pool_object = cJSON_CreateObject();
        cJSON_AddNumberToObject(pool_object, "actual_time", config[i]->polling_time);
        cJSON_AddNumberToObject(pool_object, "max", config[i]->max_polling_time);
        cJSON_AddNumberToObject(pool_object, "min", config[i]->min_polling_time);
        cJSON_AddItemToObject(sensor_object, "pool", pool_object);

        cJSON_AddBoolToObject(sensor_object, "active", config[i]->active);

        cJSON_AddItemToArray(sensors_array, sensor_object);
    }
    cJSON_AddItemToObject(payload, "sensors", sensors_array);

    char *payload_str = cJSON_Print(payload);
    cJSON_Delete(payload);
    return payload_str;
}

void new_config_message(char* action, char* type, char *payload) {
    char * message = NULL;

    cJSON *payloadObj = NULL;
    if (payload != NULL) {
        // create payload cJson
        payloadObj = cJSON_Parse(payload);
    }

    if (!strcmp(type, "config")) {
        if (!strcmp(action, "read")) {
            // Read the configuration
            // Example:
            // {
            //     "action": "read",
            //     "sender_client_id": "iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b", 
            //     "type": "config" 
            // }
            // Minified Example:
            // {"action":"read","sender_client_id":"iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b","type":"config"}

            Config_t **config = get_all_tasks_config();

            // print the config
            for (size_t i = 0; config[i] != NULL; i++) {
                ESP_LOGI("[new_config_message]", "Task: %d", config[i]->task_id);
                ESP_LOGI("[new_config_message]", "Polling Time: %d", config[i]->polling_time);
                ESP_LOGI("[new_config_message]", "Active: %d", config[i]->active);
            }

            char * payload_str = create_read_sensor_response_json(config);

            // print the payload
            ESP_LOGI("[new_config_message]", "Payload: %s", payload_str);
            asprintf(&message, "{\"action\": \"read\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": %s }", clientIdentifier, payload_str);
            free(payload_str);
        } else if (!strcmp(action, "write")) {
            // Write the configuration
            // Example: 
            // {
            //     "action": "write",
            //     "sender_client_id": "iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b", 
            //     "type": "config",
            //     "payload": { 
            //         "sensors": [{
            //             "task_id": 1,
            //             "pool": { "actual_time": 15000 },
            //             "active": true
            //         }]
            //     }
            // }
            // Minified Example:
            // {"action":"write","sender_client_id":"iotconsole-a7124307-8b16-4083-ad16-a23a60eb898b","type":"config","payload":{"sensors":[{"task_id":1,"pool":{"actual_time":15000},"active":true}]}}

            if (payloadObj == NULL) {
                ESP_LOGE("[new_config_message]", "Error parsing JSON");
                char *msg_payload;
                asprintf(&msg_payload, "\"status\": \"ok\", \"message\": \"Error parsing JSON\"");
                asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, msg_payload);
                free(msg_payload);
                return;
            }
            
            // get sensors array
            cJSON *sensors = cJSON_GetObjectItem(payloadObj,"sensors");
            // get length of sensors array
            int sensors_len = cJSON_GetArraySize(sensors);
            for (int i = 0; i < sensors_len; i++) {
                Config_t newConfig;
                cJSON *sensor_config = cJSON_GetArrayItem(sensors, i);
                newConfig.task_id = cJSON_GetObjectItem(sensor_config,"task_id")->valueint;
                ESP_LOGW("[new_config_message]", "Writing config for task id: %d", newConfig.task_id);
                newConfig.active = cJSON_GetObjectItem(sensor_config,"active")->valueint;

                cJSON *pool = cJSON_GetObjectItem(sensor_config,"pool");
                newConfig.polling_time = cJSON_GetObjectItem(pool,"actual_time")->valueint;

                // check if task_id exists
                char *task_name = get_task_name_by_id(newConfig.task_id);
                if (task_name == NULL) {
                    ESP_LOGE("[new_config_message]", "Task not found");
                    char *msg_payload;
                    asprintf(&msg_payload, "\"status\": \"ok\", \"message\": \"Task not found\"");
                    asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, msg_payload);
                    free(msg_payload);
                    return;
                }
                update_task_config(newConfig.task_id, newConfig);

                // send the ack
                char *payload;
                asprintf(&payload, "\"status\": \"ok\", \"message\": \"Configuration saved task id: %d, task name: %s\"", newConfig.task_id, task_name);
                asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, payload);
                free(payload);

                if (message != NULL) {
                    publish(create_topic("config", "dashboard", false), message);
                }
                free(message);
                message = NULL;
            }
        } else {
            ESP_LOGE("[new_config_message]", "Unknown action");
            char *msg_payload;
            asprintf(&msg_payload, "\"status\": \"ok\", \"message\": \"Unknown Action\"");
            asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, msg_payload);
            free(msg_payload);
        }
    } else {
        ESP_LOGE("[new_config_message]", "Unknown type");
        char *msg_payload;
        asprintf(&msg_payload, "\"status\": \"ok\", \"message\": \"Unknown Type\"");
        asprintf(&message, "{\"action\": \"write\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", clientIdentifier, msg_payload);
        free(msg_payload);
    }

    if (message != NULL) {
        publish(create_topic("config", "dashboard", false), message);
    }
    free(message);
}


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

    char *payload_str = NULL;
    if (payload != NULL) {
        // convert cJson payload to string
        payload_str = cJSON_Print(payload);
        ESP_LOGI("[suscriber_particular_config_handler]", "Payload: %s", payload_str);
    }

    new_config_message(action, type, payload_str);
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

    char *payload_str = cJSON_Print(payload);
    ESP_LOGI("[suscriber_particular_config_handler]", "Payload: %s", payload_str);

    new_config_message(action, type, payload_str);
}

