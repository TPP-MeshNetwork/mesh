#ifndef SENSOR_UTILS_H
#define SENSOR_UTILS_H

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "mqtt_queue.h"
#include "../mqtt/utils/mqtt_utils.h"
#include "../tasks_config/tasks_config.h"


extern char * MESH_TAG;
typedef struct {
    char * task_name;
    TaskFunction_t task_job;
    mqtt_queues_t * mqtt_queues;
} sensor_task_t;

typedef struct {
    int id;
    mqtt_queues_t *mqtt_queues;
} TaskJobArgs_t;

void create_sensor_task(char *task_name, char *sensor_type, char * sensor_metrics[], TaskFunction_t task_job , mqtt_queues_t *mqtt_queues, Config_t config, const configSTACK_DEPTH_TYPE usStackDepth);
size_t get_sensor_count(char * sensor_metrics[]);

#endif // SENSOR_UTILS_H