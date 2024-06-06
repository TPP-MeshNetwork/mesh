
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

char *create_message(char *message) {
    if (message == NULL) {
        ESP_LOGE(MESH_TAG, "Error in create_message message is NULL");
        return NULL;
    }

    // get the timestamp for the message in epoch
    time_t now;
    time(&now);

    uint8_t macAp[6];
    esp_wifi_get_mac(WIFI_IF_AP, macAp);
    char * new_message;
    asprintf(&new_message, "{\"mesh_id\": \"%s\", \"device_id\": \"" MACSTR "\", \"timestamp_value\": %lld, %s }", MESH_TAG, MAC2STR(macAp), now, message);
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