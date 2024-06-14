#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "time.h"
#include "esp_wifi.h"
#include "mqtt_queue.h"
#include "cJSON.h"
#include "../../mesh_netif/mesh_netif.h"

void publish(const char *topic, const char *message);
char * create_mqtt_message(char *message);
char * create_topic(char* topic_type, char* topic_suffix, bool withDeviceIndicator);
char * create_client_identifier();

#endif // MQTT_UTILS_H