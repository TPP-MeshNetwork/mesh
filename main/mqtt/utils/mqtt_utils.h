#ifndef MQTT_UTILS_H
#define MQTT_UTILS_H

#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "esp_mac.h"
#include "esp_log.h"
#include "time.h"
#include "esp_wifi.h"
#include "mqtt_queue.h"


void publish(const char *topic, const char *message);
char * create_message(char *message);
char * create_topic(char* topic_type, char* topic_suffix, bool withDeviceIndicator);

#endif // MQTT_UTILS_H