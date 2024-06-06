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
    Config_t *task_config_value = (Config_t *)malloc(sizeof(Config_t));
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
void update_task_config(int task_id, Config_t config) {

    TasksConfig_t *task_config;
    HASH_FIND_INT(tasks_config, &task_id, task_config);
    ESP_LOGI("[update_task_config]", "Updating task id: %d config", task_id);

    // create a new config
    Config_t *config_ = (Config_t *) malloc(sizeof(Config_t));
    config_->polling_time = config.polling_time;
    config_->active = config.active;

    Config_t old_config;
    // remove the old config
    xQueueReceive(task_config->config, &old_config, 0);

    // do not change min or max polling time
    config_->min_polling_time = old_config.min_polling_time;
    config_->max_polling_time = old_config.max_polling_time;


    // send the new config to the queue
    xQueueSend(task_config->config, config_, 0);

    if (old_config.polling_time != config.polling_time) {
        ESP_LOGI("[update_task_config]", "Updating polling_time: %d", config.polling_time);
    
    }
    if (old_config.active != config.active) {
        ESP_LOGI("[update_task_config]", "Updating active: %d", config.active);
    }
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
int add_task_mapping(char * task_name, char * sensor_metrics[]) {
    // check if the task_name already exists
    if (get_task_id_by_name(task_name) != -1) {
        return -1;
    }

    TasksMapping_t *ret_task_mapping = (TasksMapping_t *)malloc(sizeof(TasksMapping_t));
    ret_task_mapping->id = HASH_COUNT(tasks_mapping) + 1;
    strcpy(ret_task_mapping->task_name, task_name);

    // creating a copy in the heap of the sensor_metrics
    int i = 0;
    while (sensor_metrics[i] != NULL) i++; // count how many items are in the array
    char ** sensor_metrics_copy = (char **)malloc(sizeof(char *) * i + 1); // +1 for the NULL
    for (int j = 0; j < i; j++) {
        sensor_metrics_copy[j] = (char *) malloc(sizeof(char) * strlen(sensor_metrics[j]) + 1);
        strcpy(sensor_metrics_copy[j], sensor_metrics[j]);
        sensor_metrics_copy[j][strlen(sensor_metrics[j])] = '\0';
    }
    sensor_metrics_copy[i] = NULL; // adding the NULL at the end of the array

    ret_task_mapping->sensor_types = sensor_metrics_copy;
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
    return ret_task_mapping->sensor_types;
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

        // Creating an array for sensor metrics
        cJSON * sensor_metrics_array = cJSON_CreateArray();
        char ** sensor_metrics_ = task_mapping->sensor_types;
        for (int i = 0; sensor_metrics_[i] != NULL; i++) {
            cJSON_AddItemToArray(sensor_metrics_array, cJSON_CreateString(sensor_metrics_[i]));
        }
        cJSON_AddItemToObject(task_object, "metrics", sensor_metrics_array);
        cJSON_AddItemToArray(tasks_array, task_object);
    }

    // Create a JSON object to hold the tasks array
    cJSON * tasks_object = cJSON_CreateObject();
    cJSON_AddItemToObject(tasks_object, "tasks", tasks_array);

    return tasks_object;
}
