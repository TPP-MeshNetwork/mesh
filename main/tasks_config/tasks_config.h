#ifndef TASKS_CONFIG_H
#define TASKS_CONFIG_H


#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "string.h"
#include "../utils/uthash.h"
#include "nvs_flash.h"
#include "cJSON.h"

#define TASKS_NAME_SIZE 255
#define TASKS_CONFIG_PERSISTENCE_NAMESPACE "tasks_config"

typedef struct {
    size_t task_id; // The task id
    size_t max_polling_time; // The maximum time to wait for the next polling
    int min_polling_time; // The minimum time to wait for the next polling
    size_t polling_time; // The actual ammount of time to wait for the next polling
    int active; // If the task is active
} Config_t;

typedef struct {
    int task_id;                    /* key */
    QueueHandle_t config;                        /* value: queue of 1 element to handle the peek and update of config */
    UT_hash_handle hh;
} TasksConfig_t;

typedef struct sensor_metric{
    char * metric_type;
    char * metric_unit;
    struct sensor_metric * next;
} SensorMetric_t;

typedef struct {
    char task_name[TASKS_NAME_SIZE]; /* key */
    int id;
    char * sensor_name;
    SensorMetric_t *sensor_metrics; // Metrics list
    UT_hash_handle hh;
} TasksMapping_t;


// config functions
void add_task_config(int task_id, char * type, Config_t config);
Config_t * get_task_config(int task_id);
Config_t* update_task_config(int task_id, Config_t config);
void validate_task_config(Config_t *config);
Config_t ** get_all_tasks_config();
void save_task_config(int task_id);
esp_err_t load_task_config(int task_id);

// tasks mapping functions
int get_task_id_by_name(char * task_name);
char * get_task_name_by_id(int id);
int add_task_mapping(char * task_name, char * sensor_name);

// sensor metrics functions
void add_sensor_metric(char* task_name, char * metric_type, char * metric_unit);

char ** get_sensor_metrics_by_task_id(int id);
cJSON * get_all_tasks_metrics_json();

int ** get_all_tasks_ids();


#endif // TASKS_CONFIG_H
