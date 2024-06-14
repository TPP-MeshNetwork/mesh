
#include "mqtt_utils.h"

extern mqtt_queues_t *mqtt_queues;
extern char *MESH_TAG;
void publish(const char *topic, const char *message) {
    if(mqtt_queues == NULL) {
        ESP_LOGE(MESH_TAG, "Error in publish: mqtt_queues is NULL");
        return;
    }
    QueueHandle_t publishQueue = mqtt_queues->mqttPublisherQueue;
    mqtt_message_t mqtt_message;
    strcpy(mqtt_message.topic, topic);
    strcpy(mqtt_message.message, message);
    xQueueSend(publishQueue, &mqtt_message, 0);
}

cJSON * merge_json_objects(cJSON *a, cJSON *b) {
    cJSON *res = cJSON_CreateObject();
    if (res == NULL) {
        // Handle error
        return NULL;
    }

    // Merge elements from 'a'
    cJSON *node = NULL;
    cJSON_ArrayForEach(node, a) {
        cJSON_AddItemToObject(res, node->string, cJSON_Duplicate(node, 1));
    }

    // Merge elements from 'b'
    cJSON_ArrayForEach(node, b) {
        cJSON_AddItemToObject(res, node->string, cJSON_Duplicate(node, 1));
    }

    return res;
}

char * create_mqtt_message(char *message) {
    if (message == NULL) {
        ESP_LOGE(MESH_TAG, "Error in create_mqtt_message message is NULL");
        return NULL;
    }

    // get the timestamp for the message in epoch
    time_t now;
    time(&now);

    uint8_t macAp[6];
    esp_wifi_get_mac(WIFI_IF_AP, macAp);


    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "mesh_id", MESH_TAG);
    char * macApStr = get_mac_ap();
    cJSON_AddStringToObject(root, "device_id", macApStr);
    cJSON_AddNumberToObject(root, "timestamp_value", now);
    cJSON *json_message = cJSON_Parse(message);
    free(macApStr);

    if (json_message == NULL) {
        ESP_LOGE(MESH_TAG, "Error in create_message: json_message is NULL");
        cJSON_Delete(root);
        return NULL;
    }

    // add json_message in the same level as root merged = root Note: do not free root
    cJSON *merged = merge_json_objects(root, json_message);
    cJSON_Delete(root);
    cJSON_Delete(json_message);

    if (merged == NULL) {
        return NULL;
    }

    char *new_message = cJSON_PrintUnformatted(merged);
    cJSON_Delete(merged);
    return new_message;
}

char * create_topic(char* topic_type, char* topic_suffix, bool withDeviceIndicator) {
    if (topic_type == NULL || topic_suffix == NULL) {
        ESP_LOGE(MESH_TAG, "Error in create_topic: topic_type or topic_suffix is NULL");
        return NULL;
    }
    uint8_t macAp[6];
    esp_wifi_get_mac(WIFI_IF_AP, macAp);
    char *topic;
    if (withDeviceIndicator) {
        if (strcmp(topic_suffix, "") == 0)
          asprintf(&topic, "/mesh/%s/devices/" MACSTR "/%s", MESH_TAG, MAC2STR(macAp), topic_type);
        else
          asprintf(&topic, "/mesh/%s/devices/" MACSTR "/%s/%s", MESH_TAG, MAC2STR(macAp), topic_type, topic_suffix);
    } else if (strcmp(topic_suffix, "") == 0)
        asprintf(&topic, "/mesh/%s/%s", MESH_TAG, topic_type);
    else {
        asprintf(&topic, "/mesh/%s/%s/%s", MESH_TAG, topic_type, topic_suffix);
    }
    return topic;
}

char * create_client_identifier() {
    return get_mac_ap();
}