#ifndef SENSOR_UTILS_H
#define SENSOR_UTILS_H

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "mqtt_queue.h"
#include "../mqtt/utils/mqtt_utils.h"
////////// Sensors Includes /////////
// // Sensor: DH11
// #include <components/dht.h>
// #define SENSOR_TYPE DHT_TYPE_DHT11


extern char * MESH_TAG;
typedef struct {
    char * task_name;
    TaskFunction_t task_job;
    mqtt_queues_t * mqtt_queues;
    // config for the task
} task_config_t;

void create_sensor_task(char *task_name, TaskFunction_t task_job , mqtt_queues_t *mqtt_queues);
void task_job_guard(void *args);

#endif // SENSOR_UTILS_H