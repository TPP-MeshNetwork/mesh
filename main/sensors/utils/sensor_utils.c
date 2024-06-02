#include "sensor_utils.h"

/*
  * Function: create_sensor_task
  * ----------------------------
  *   Creates a new sensor task
  *
  */
void create_sensor_task(char *task_name, TaskFunction_t task_job , mqtt_queues_t *mqtt_queues) {

    task_config_t *task_config = malloc(sizeof(task_config_t));
    task_config->task_job = task_job; // adding task job so that it can be executed with the guard task
    task_config->task_name = task_name;
    task_config->mqtt_queues = mqtt_queues;
    // TODO: ADD CONFIG FOR THE TASK

    xTaskCreate(task_job_guard, task_name, 3072, (void *) task_config, 5, NULL);
}

/*
  * Function: task_job_guard
  * ----------------------------
  *   Decorator wrapper for the sensor task
  *
  */
static void task_job_guard(void *args) {
    task_config_t *task_config = (task_config_t *) args;
    ESP_LOGI(MESH_TAG, "STARTED: %s", task_config->task_name);
    // execute task job
    task_config->task_job((void *) task_config->mqtt_queues);
    free(task_config);
}
