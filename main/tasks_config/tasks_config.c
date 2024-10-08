/* This file contains functions to handle the config of the tasks
*/

#include "tasks_config.h"

TasksConfig_t *tasks_config = NULL;
TasksMapping_t * tasks_mapping = NULL;

/*
  * Function: add_task_config
    * ----------------------------
    * Add a task config to the tasks_config
    * 
    * task_id: The task id
    * type: The type of the task
    * config: The config of the task
    *   
    * returns: void
*/
void add_task_config(int task_id, char * type, Config_t config) {
    // check if the task_id already exists
    TasksConfig_t *check_task_config;
    HASH_FIND_INT(tasks_config, &task_id, check_task_config);
    if (check_task_config != NULL) {
        ESP_LOGI("[add_task_config]", "Task id: %d already exists", task_id);
        return;
    }

    Config_t *config_ = (Config_t *) malloc(sizeof(Config_t));
    config_->task_id = task_id;
    config_->max_polling_time = config.max_polling_time;
    config_->min_polling_time = config.min_polling_time;
    config_->polling_time = config.polling_time;
    config_->active = config.active;
    ESP_LOGI("[add_task_config]", "Adding config for task id: %d type: %s", config_->task_id, type);

    TasksConfig_t *task_config = (TasksConfig_t *)malloc(sizeof(TasksConfig_t));
    task_config->task_id = task_id;
    task_config->config = xQueueCreate(1, sizeof(Config_t));
    xQueueSend(task_config->config, &config, 0);

    HASH_ADD_INT(tasks_config, task_id, task_config);
}

/*
    * Function: get_task_config
    * ----------------------------
    *   Get a task config from the tasks_config
    *
    * task_id: The task id
    *   
    * returns: Config_t
*/
Config_t * get_task_config(int task_id) {

    TasksConfig_t *task_config;
    HASH_FIND_INT(tasks_config, &task_id, task_config);
    
    if (task_config == NULL) {
      return NULL;
    }
    // peek the first value of the queue
    Config_t *task_config_value = (Config_t *) malloc(sizeof(Config_t));
    xQueuePeek(task_config->config, task_config_value, 0);
    return task_config_value;
}

/*
    * Function: get_all_tasks_config
    * ----------------------------
    *   Get all the tasks config from the tasks_config
    *   
    * returns: Config_t **
*/
Config_t ** get_all_tasks_config() {
    if (HASH_COUNT(tasks_config) == 0) {
        return NULL;
    }
    Config_t **tasks_config_values = (Config_t **) malloc(sizeof(Config_t *) * HASH_COUNT(tasks_config) + 1);
    TasksConfig_t *task_config, *tmp;
    int i = 0;
    HASH_ITER(hh, tasks_config, task_config, tmp) {
        // Debug: check if queue is empty
        if (uxQueueMessagesWaiting(task_config->config) == 0) {
            printf("Queue for task config %d is empty\n", i);
            continue;
        }
        tasks_config_values[i] = (Config_t *) malloc(sizeof(Config_t));
        xQueuePeek(task_config->config, tasks_config_values[i], 10);
        tasks_config_values[i]->task_id = task_config->task_id; // FIX: patch for the task_id (dont know why it is not being copied)
        ESP_LOGI("[get_all_tasks_config]", "Task: %d", task_config->task_id);
        i++;
    }
    tasks_config_values[i] = NULL;
    return tasks_config_values;
}

/*
    * Function: update_task_config
    * ----------------------------
    *   Update a task config from the tasks_config
    *   
    * task_id: The task id
    * config: The config of the task

*/
Config_t* update_task_config(int task_id, Config_t new_config) {

    TasksConfig_t *task_config;
    HASH_FIND_INT(tasks_config, &task_id, task_config);
    ESP_LOGI("[update_task_config]", "Updating task id: %d config", task_id);

    // create a new config
    Config_t *config = (Config_t *) malloc(sizeof(Config_t));

    Config_t old_config;
    // remove the old config
    xQueueReceive(task_config->config, &old_config, 0);

    // copy old values onto config
    config->task_id = task_id;
    config->max_polling_time = old_config.max_polling_time;
    config->min_polling_time = old_config.min_polling_time;
    config->polling_time = new_config.polling_time;
    config->active = new_config.active;
    
    // set new values for polling_time and active
    old_config.polling_time = new_config.polling_time;
    old_config.active = new_config.active;
    
    // validate the new config
    validate_task_config(config);

    // send the new config to the queue
    xQueueSend(task_config->config, config, 0);

    if (old_config.polling_time != new_config.polling_time) {
        ESP_LOGI("[update_task_config]", "Updating polling_time: %d", new_config.polling_time);
    
    }
    if (old_config.active != new_config.active) {
        ESP_LOGI("[update_task_config]", "Updating active: %d", new_config.active);
    }

    return config;
}

/*
    * Function: save_task_config
    * ----------------------------
    *   Save a task config to the tasks_config
    *   
    * task_id: The task id

*/
void save_task_config(int task_id) {
    TasksConfig_t *task_config;
    HASH_FIND_INT(tasks_config, &task_id, task_config);
    Config_t *config = (Config_t *) malloc(sizeof(Config_t));
    xQueuePeek(task_config->config, config, 0);
    ESP_LOGI("[save_task_config]", "Saving task id: %d config", task_id);
    // save the config to the flash
    // open a new nvs namespace
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open(TASKS_CONFIG_PERSISTENCE_NAMESPACE, NVS_READWRITE, &my_handle));
    char key[10];
    sprintf(key, "%d", task_id);
    ESP_ERROR_CHECK(nvs_set_blob(my_handle, key, config, sizeof(Config_t)));
    ESP_ERROR_CHECK(nvs_commit(my_handle));
    nvs_close(my_handle);
}

/*
    * Function: load_task_config
    * ----------------------------
    *   Load a task config from the tasks_config
    *   
    * task_id: The task id

*/
esp_err_t load_task_config(int task_id) {
    // open a new nvs namespace
    nvs_handle_t my_handle;
    ESP_ERROR_CHECK(nvs_open(TASKS_CONFIG_PERSISTENCE_NAMESPACE, NVS_READWRITE, &my_handle));
    char key[10];
    sprintf(key, "%d", task_id);
    Config_t *config = (Config_t *) malloc(sizeof(Config_t));
    size_t required_size;
    // omitting the ESP_ERR_NVS_NOT_FOUND error
    if (nvs_get_blob(my_handle, key, NULL, &required_size) != ESP_OK) {
        ESP_LOGI("[load_task_config]", "Task id: %d config not found", task_id);
        nvs_close(my_handle);
        return ESP_ERR_NVS_NOT_FOUND;
    }
    ESP_ERROR_CHECK(nvs_get_blob(my_handle, key, config, &required_size));
    nvs_close(my_handle);
    ESP_LOGI("[load_task_config]", "Loading task id: %d config", task_id);
    // update the task config
    update_task_config(task_id, *config);
    free(config);
    return ESP_OK;
}

/*
    * Function: validate_task_config
    * ----------------------------
    *   Validate the task config
    *   
    * config: The config of the task

*/
void validate_task_config(Config_t *config) {
    if (config->polling_time < config->min_polling_time) {
        ESP_LOGI("[check_task_config]", "Polling time is less than min_polling_time %d, default will be min_polling_time %d", config->polling_time, config->min_polling_time);
        config->polling_time = config->min_polling_time;
    } 
    // if max_polling_time is 0, it means that there is no max_polling_time
    if (config->polling_time > config->max_polling_time && config->max_polling_time != 0) {
        ESP_LOGI("[check_task_config]", "Polling time %d is greater than max_polling_time %d, default will be max_polling_time", config->polling_time, config->max_polling_time);
        config->polling_time = config->max_polling_time;
    }
    if (config->active != 0 && config->active != 1) {
        ESP_LOGI("[check_task_config]", "Active is not 0 or 1, disabling the sensor task");
        config->active = 0;
    }
}

/*
  * Function: get_task_id_by_name
  * ----------------------------
  *   Get the task id by name
  *
*/
int get_task_id_by_name(char * task_name) {
    TasksMapping_t * ret_task_mapping;
    HASH_FIND_STR(tasks_mapping, task_name, ret_task_mapping);
    if (ret_task_mapping == NULL) {
        return -1;
    }
    return ret_task_mapping->id;
}

/*
  * Function: add_task_mapping
  * ----------------------------
  *   Add a task mapping
  *
*/
int add_task_mapping(char * task_name, char *sensor_name) {
    // check if the task_name already exists
    if (get_task_id_by_name(task_name) != -1) {
        return -1;
    }

    TasksMapping_t *ret_task_mapping = (TasksMapping_t *) malloc(sizeof(TasksMapping_t));
    ret_task_mapping->id = HASH_COUNT(tasks_mapping) + 1;
    strcpy(ret_task_mapping->task_name, task_name);

    // // Creating a copy in the heap of the sensor_metrics
    // int i = 0;
    // while (sensor_metrics[i] != NULL) i++; // count how many items are in the array
    // char ** sensor_metrics_copy = (char **)malloc(sizeof(char *) * i + 1); // +1 for the NULL
    // for (int j = 0; j < i; j++) {
    //     sensor_metrics_copy[j] = (char *) malloc(sizeof(char) * strlen(sensor_metrics[j]) + 1);
    //     strcpy(sensor_metrics_copy[j], sensor_metrics[j]);
    //     sensor_metrics_copy[j][strlen(sensor_metrics[j])] = '\0';
    // }
    // sensor_metrics_copy[i] = NULL; // adding the NULL at the end of the array

    // Adding sensor name
    ret_task_mapping->sensor_name = (char *) malloc(sizeof(char) * strlen(sensor_name) + 1);
    strcpy(ret_task_mapping->sensor_name, sensor_name);
    ret_task_mapping->sensor_name[strlen(sensor_name)] = '\0';

    ret_task_mapping->sensor_metrics = NULL;
    ESP_LOGI("[add_task_mapping]", "Adding task mapping: %s, id: %d", task_name, ret_task_mapping->id);

    HASH_ADD_STR(tasks_mapping, task_name, ret_task_mapping);

    return ret_task_mapping->id;
}

/*
  * Function: get_task_name_by_id
  * ----------------------------
  *  Get the task name by id
  *   
  * returns: sensor name
*/
char * get_task_name_by_id(int id) {
    TasksMapping_t *ret_task_mapping;
    // iterate the task_mapping to find the task_name
    for(ret_task_mapping = tasks_mapping; ret_task_mapping != NULL; ret_task_mapping = (TasksMapping_t*)(ret_task_mapping->hh.next)) {
        if (ret_task_mapping->id == id) {
            return ret_task_mapping->task_name;
        }
    }
    return NULL;
}


/*
    * Function: get_sensor_metrics_by_task_id
    * ----------------------------
    *  Get the sensor metrics by task id
    *   
    * returns: sensor metrics char **
*/
char ** get_sensor_metrics_by_task_id(int id) {
    char * task_name = get_task_name_by_id(id);
    TasksMapping_t *ret_task_mapping;
    HASH_FIND_STR(tasks_mapping, task_name, ret_task_mapping);
    if (ret_task_mapping == NULL) {
        return NULL;
    }
    SensorMetric_t *sensor_metrics = ret_task_mapping->sensor_metrics;
    if (sensor_metrics == NULL) {
        return NULL;
    }
    char ** sensor_metrics_ = (char **) malloc(sizeof(char *) * 5);
    int i = 0;
    while (sensor_metrics != NULL) {

        // check if we need more memory for the array
        if (i > 5) {
            sensor_metrics_ = (char **) realloc(sensor_metrics_, sizeof(char *) * i + 1);
        }
        sensor_metrics_[i] = (char *) malloc(sizeof(char) * strlen(sensor_metrics->metric_type) + 1);
        strcpy(sensor_metrics_[i], sensor_metrics->metric_type);
        sensor_metrics_[i][strlen(sensor_metrics->metric_type)] = '\0';
        sensor_metrics = sensor_metrics->next;
        i++;
    }
    sensor_metrics_[i] = NULL;
    return sensor_metrics_;
}

/*
    * Function: get_all_tasks_ids
    * ----------------------------
    *  Get all the tasks ids
    *   
    * returns: int **
*/
int ** get_all_tasks_ids() {
    if (HASH_COUNT(tasks_mapping) == 0) {
        return NULL;
    }
    int **tasks_ids = (int **) malloc(sizeof(int *) * HASH_COUNT(tasks_mapping) + 1);
    TasksMapping_t *task_mapping, *tmp;
    int i = 0;
    HASH_ITER(hh, tasks_mapping, task_mapping, tmp) {
        tasks_ids[i] = (int *) malloc(sizeof(int));
        tasks_ids[i] = &task_mapping->id;
        i++;
    }
    tasks_ids[i] = NULL;
    return tasks_ids;
}

/*
    * Function: get_all_tasks_metrics_json
    * ----------------------------
    *  Get all the tasks metrics in json format
    *   
    * returns: cJSON *
*/
cJSON * get_all_tasks_metrics_json() {
    cJSON *tasks_array = cJSON_CreateArray();

    TasksMapping_t *task_mapping, *tmp;
    HASH_ITER(hh, tasks_mapping, task_mapping, tmp) {
        cJSON * task_object = cJSON_CreateObject();

        // Adding task ID
        cJSON_AddItemToObject(task_object, "id", cJSON_CreateNumber(task_mapping->id));

        // Adding task name
        cJSON_AddItemToObject(task_object, "name", cJSON_CreateString(task_mapping->task_name));

        // Adding sensor name
        cJSON_AddItemToObject(task_object, "sensor", cJSON_CreateString(task_mapping->sensor_name));

        // Creating an array for sensor metrics
        cJSON * sensor_metrics_array = cJSON_CreateArray();
        SensorMetric_t* sensor_metrics_ = task_mapping->sensor_metrics;
        while (sensor_metrics_ != NULL) {
            cJSON *sensor_metric = cJSON_CreateObject();
            cJSON_AddItemToObject(sensor_metric, "type", cJSON_CreateString(sensor_metrics_->metric_type));
            cJSON_AddItemToObject(sensor_metric, "unit", cJSON_CreateString(sensor_metrics_->metric_unit));
            cJSON_AddItemToArray(sensor_metrics_array, sensor_metric);
            sensor_metrics_ = sensor_metrics_->next;
        }
        cJSON_AddItemToObject(task_object, "metrics", sensor_metrics_array);
        cJSON_AddItemToArray(tasks_array, task_object);
    }
    return tasks_array;
}

void add_sensor_metric(char* task_name, char * metric_type, char * metric_unit) {
    TasksMapping_t *ret_task_mapping;
    HASH_FIND_STR(tasks_mapping, task_name, ret_task_mapping);
    if (ret_task_mapping == NULL) {
        ESP_LOGI("[add_sensor_metric]", "Task name: %s not found", task_name);
        return;
    }
    SensorMetric_t *sensor_metric = (SensorMetric_t *) malloc(sizeof(SensorMetric_t));
    sensor_metric->metric_type = (char *) malloc(sizeof(char) * strlen(metric_type) + 1);
    strcpy(sensor_metric->metric_type, metric_type);
    sensor_metric->metric_type[strlen(metric_type)] = '\0';
    sensor_metric->metric_unit = (char *) malloc(sizeof(char) * strlen(metric_unit) + 1);
    strcpy(sensor_metric->metric_unit, metric_unit);
    sensor_metric->metric_unit[strlen(metric_unit)] = '\0';

    if (ret_task_mapping->sensor_metrics == NULL) {
        // First metric, add the current metric to the set
        ret_task_mapping->sensor_metrics = sensor_metric;
        sensor_metric->next = NULL;
    } else {
        // Is not the first metric, add the current metric to the last position
        SensorMetric_t *sensor_metrics_ = ret_task_mapping->sensor_metrics;
        while (sensor_metrics_->next != NULL) {
            sensor_metrics_ = sensor_metrics_->next;
        }
        sensor_metrics_->next = sensor_metric;
        sensor_metric->next = NULL;
    }

    //sensor_metric->next = ret_task_mapping->sensor_metrics;
    //ret_task_mapping->sensor_metrics = sensor_metric;
   
    ESP_LOGI("[add_sensor_metric]", "Adding sensor metric: %s (unit: %s) to task name: %s", metric_type, metric_unit, task_name);
}
