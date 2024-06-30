#include "sensor_utils.h"

/*
  * Function: task_job_guard
  * ----------------------------
  *   Decorator wrapper for the sensor task
  *
  */
static void task_job_guard(void *args) {
    sensor_task_t *sensor_task = (sensor_task_t *) args;
    ESP_LOGI(MESH_TAG, "STARTED: %s", sensor_task->task_name);

    // get task args
    TaskJobArgs_t *task_args = malloc(sizeof(TaskJobArgs_t));
    task_args->id = get_task_id_by_name(sensor_task->task_name);
    task_args->mqtt_queues = sensor_task->mqtt_queues;
    // execute task job
    sensor_task->task_job((void *) task_args);
    free(sensor_task);
}

/*
  * Function: create_sensor_task
  * ----------------------------
  *   Creates a new sensor task
  *
  */
void create_sensor_task(char *task_name, char * sensor_type, char * sensor_metrics[], char* sensor_units[], TaskFunction_t task_job , mqtt_queues_t *mqtt_queues, Config_t config, const configSTACK_DEPTH_TYPE usStackDepth) {

    sensor_task_t *sensor_task = malloc(sizeof(sensor_task_t));
    sensor_task->task_job = task_job; // adding task job so that it can be executed with the guard task
    sensor_task->task_name = task_name;
    sensor_task->mqtt_queues = mqtt_queues;

    // add task mapping task_name to id
    int task_id = add_task_mapping(task_name, sensor_type);
    
    if (task_id == -1) {
        ESP_LOGI(MESH_TAG, "Task mapping already exists");
        return;
    }

    // add sensor metrics
    for (size_t i = 0; sensor_metrics[i] != NULL; i++) {
        char * metric_type = sensor_metrics[i];
        char * metric_unit = sensor_units[i];
        if (metric_type == NULL) {
            ESP_LOGE(MESH_TAG, "Error in create_sensor_task: metric_type or metric_unit is NULL");
        } else {
            add_sensor_metric(task_name, metric_type, metric_unit!=NULL ? metric_unit : "");
        }
    }

    // adding task config to the tasks_config
    add_task_config(task_id, sensor_type, config);

    // load values from nvs if exists
    esp_err_t ret = load_task_config(task_id);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        // Trying to save the config 
        save_task_config(task_id);
    } else {
        ESP_LOGI(MESH_TAG, "Loaded config from NVS");
    }

    xTaskCreate(task_job_guard, task_name, usStackDepth, (void *) sensor_task, 5, NULL);
}

/*
  * Function: get_sensor_count
  * ----------------------------
  *   Get the sensor count
  *
*/
size_t get_sensor_count(char * sensor_metrics[]) {
    size_t count = 0;
    for (size_t i = 0; sensor_metrics[i] != NULL; i++) {
        count++;
    }
    return count;
}



