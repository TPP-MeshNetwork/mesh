/* 
*   This file contains the event handlers for the suscriptions
*   Particular Topic: /mesh/[mesh_id]/devices/[device_id]/config
*   Global Topic: /mesh/[mesh_id]/config
*   Publishes on different topic: /mesh/[mesh_id]/config/dashboard
*/
#include "suscription_event_handlers.h"

extern char * clientIdentifier;
extern mqtt_queues_t mqtt_queues;
extern char * MESH_TAG;

char * create_message_config(char* action, cJSON* payload) {
    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "type", "config");
    cJSON_AddStringToObject(root, "action", action);
    cJSON_AddStringToObject(root, "sender_client_id", clientIdentifier);
    cJSON *firmware = cJSON_CreateObject();
    cJSON_AddStringToObject(firmware, "version", FIRMWARE_VERSION);
    cJSON_AddItemToObject(root, "firmware", firmware);

    ESP_LOGI(MESH_TAG, "%s", cJSON_Print(payload));

    if (payload != NULL)
        cJSON_AddItemToObject(root, "payload", payload);
    
    char *message = cJSON_PrintUnformatted(root);
    cJSON_free(root);
    return message;

}

cJSON* make_pool_object(Config_t *config) {
    cJSON *pool_object = cJSON_CreateObject();
    cJSON_AddNumberToObject(pool_object, "actual_time", config->polling_time);
    cJSON_AddNumberToObject(pool_object, "max", config->max_polling_time);
    cJSON_AddNumberToObject(pool_object, "min", config->min_polling_time);
    return pool_object;
}

cJSON* make_sensors_item_object(Config_t *config) {
    cJSON *item = cJSON_CreateObject();
    cJSON_AddNumberToObject(item, "task_id", config->task_id);
    cJSON *pool_object = make_pool_object(config);
    cJSON_AddItemToObject(item, "pool", pool_object);
    cJSON_AddBoolToObject(item, "active", config->active);
    return item;
}

cJSON * create_read_sensor_response_json(Config_t *config[]) {
    // Create the payload object
    cJSON *payload = cJSON_CreateObject();
    cJSON *sensors_array = cJSON_CreateArray();

    // Populate sensors array
    for (size_t i = 0; config[i] != NULL; i++) {
        cJSON *sensor_object = make_sensors_item_object(config[i]);

        cJSON_AddItemToArray(sensors_array, sensor_object);
    }
    cJSON_AddItemToObject(payload, "sensors", sensors_array);
    return payload;
}

void new_config_message(char* action, char* type, char *payload) {
    char * message = NULL;

    cJSON *payloadObj = NULL;
    if (payload != NULL) {
        // create payload cJson
        payloadObj = cJSON_Parse(payload);
    }

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

        if (config == NULL) {
            ESP_LOGE("[new_config_message]", "Error getting config");
            cJSON *payloadRet = cJSON_CreateObject();
            cJSON_AddStringToObject(payloadRet, "status", "error");
            cJSON_AddStringToObject(payloadRet, "message", "Error getting config");

            char * msg_read = create_message_config("read", payloadRet);
            message = create_mqtt_message(msg_read);
            publish(create_topic("config", "dashboard", false), message);
            free(message);
            free(msg_read);
            return;
        }

        // print the config
        for (size_t i = 0; config[i] != NULL; i++) {
            ESP_LOGI("[new_config_message]", "Task: %d", config[i]->task_id);
            ESP_LOGI("[new_config_message]", "Polling Time: %d", config[i]->polling_time);
            ESP_LOGI("[new_config_message]", "Active: %d", config[i]->active);
        }

        cJSON * payloadRet = create_read_sensor_response_json(config);

        char * msg_read = create_message_config("read", payloadRet);
        message = create_mqtt_message(msg_read);
        free(msg_read);
        cJSON_Delete(payloadRet);

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
            cJSON *payloadRet = cJSON_CreateObject();
            cJSON_AddStringToObject(payloadRet, "status", "error");
            cJSON_AddStringToObject(payloadRet, "message", "Error parsing JSON");

            char * msg_write = create_message_config("write", payloadRet);
            message = create_mqtt_message(msg_write);
            publish(create_topic("config", "dashboard", false), message);
            free(message);
            free(msg_write);
            cJSON_Delete(payloadRet);
            return;
        }
        
        // get sensors array
        cJSON *sensors = cJSON_GetObjectItem(payloadObj,"sensors");
        // get length of sensors array
        int sensors_len = cJSON_GetArraySize(sensors);

        // create the sensors response object array
        cJSON *payloadRet = cJSON_CreateObject();
        cJSON *sensors_array = cJSON_CreateArray();

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
                ESP_LOGE("[new_config_message]", "Task %d not found", newConfig.task_id);  
                cJSON *sensor_object = cJSON_CreateObject();
                cJSON_AddNumberToObject(sensor_object, "task_id", newConfig.task_id);
                cJSON_AddStringToObject(sensor_object, "status", "error");
                cJSON_AddStringToObject(sensor_object, "message", "Task not found");
                cJSON_AddItemToArray(sensors_array, sensor_object);
                continue;
            }
            // Update the task config with new values
            Config_t *settedConfig = update_task_config(newConfig.task_id, newConfig);

            // Create sensor object for the response
            cJSON *sensor_object = make_sensors_item_object(settedConfig);

            // Set the status anb the message for the sensor object
            cJSON_AddStringToObject(sensor_object, "status", "ok");
            char * message_str;
            asprintf(&message_str, "Configuration saved task id: %d, task name: %s", newConfig.task_id, task_name);
            cJSON_AddStringToObject(sensor_object, "message", message_str);
            free(message_str);
            cJSON_AddItemToArray(sensors_array, sensor_object);
        }

        cJSON_AddItemToObject(payloadRet, "sensors", sensors_array);
        char * msg_write = create_message_config("write", payloadRet);
        message = create_mqtt_message(msg_write);
        cJSON_Delete(payloadRet);
        free(msg_write);
    } else {
        ESP_LOGE("[new_config_message]", "Unknown action");
        cJSON *payloadRet = cJSON_CreateObject();
        cJSON_AddStringToObject(payloadRet, "status", "error");
        cJSON_AddStringToObject(payloadRet, "message", "Unknown Action");
        char * msg_write = create_message_config("write", payloadRet);
        message = create_mqtt_message(msg_write);
        free(msg_write);
        cJSON_Delete(payloadRet);

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

    if (!strcmp(type, "config")) {
        new_config_message(action, type, payload_str);
    } else {
        ESP_LOGE("[suscriber_particular_config_handler]", "Unknown type");
        cJSON *sensor_object = cJSON_CreateObject();
        cJSON_AddStringToObject(sensor_object, "status", "error");
        cJSON_AddStringToObject(sensor_object, "message", "Unknown Type");
        char *msg_payload = cJSON_Print(sensor_object);  
        asprintf(&message, "{\"action\": \"%s\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": %s }", action, clientIdentifier, msg_payload);
        publish(create_topic("config", "dashboard", true), message);
        free(msg_payload);
    }
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

    // get the payload only if the action is write
    char *payload_str = NULL;
    if (!strcmp(action, "write")) {
        // get the payload
        cJSON *payload = cJSON_GetObjectItem(root,"payload");
        payload_str = cJSON_Print(payload);
        ESP_LOGI("[suscriber_particular_config_handler]", "Payload: %s", payload_str);
    }

    if (!strcmp(type, "config")) {
        new_config_message(action, type, payload_str);
    } else {
        ESP_LOGE("[suscriber_global_config_handler]", "Unknown type");
        char *msg_payload;
        asprintf(&msg_payload, "\"status\": \"ok\", \"message\": \"Unknown Type\"");
        asprintf(&message, "{\"action\": \"%s\", \"sender_client_id\": \"%s\", \"type\": \"config\", \"payload\": {%s}}", action, clientIdentifier, msg_payload);
        publish(create_topic("config", "dashboard", false), message);
        free(msg_payload);
    }
}

