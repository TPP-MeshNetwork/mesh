#include "../utils/sensor_utils.h"
#include "tasks_config.h"
// Sensor Name: ESP32 Performance

void task_sensor_performance(void *args) {
    TaskJobArgs_t * args_ = (TaskJobArgs_t *)args;
    mqtt_queues_t *mqtt_queues = args_->mqtt_queues;
    int job_id = args_->id;
    char *sensor_message = NULL;

    // Getting metrics from task_id configured
    char ** sensor_metrics = get_sensor_metrics_by_task_id(job_id);

    // Getting the sensor metrics count
    size_t sensor_length = get_sensor_count(sensor_metrics);

    // Creating the response topics from the metrics
    char * sensor_topic[sensor_length];
    for (size_t i = 0; i < sensor_length; i++) {
        sensor_topic[i] = create_topic("sensor", sensor_metrics[i], true);
    }

    uint32_t sensor_data[] = {0, 0, 0};

    while (1) {
        //////// CONFIG - DO NOT TOUCH THIS
            Config_t *config = get_task_config(job_id);
            validate_task_config(config);
            if (config->active == 0) {
                vTaskDelay(pdMS_TO_TICKS(1000)); // 1 second delay to get new config
                free(config);
                continue;
            }
        //////// CONFIG - END
       
        // ESP_LOGI(MESH_TAG, "Reading memory usage");
        // ESP_LOGI(MESH_TAG, "Free memory: %d bytes", esp_get_free_heap_size());
        sensor_data[0] = esp_get_free_heap_size();
        // ESP_LOGI(MESH_TAG, "Minimun free memory: %d bytes", esp_get_minimum_free_heap_size());
        sensor_data[1] = esp_get_minimum_free_heap_size();
        // ESP_LOGI(MESH_TAG, "Memory usage: %d bytes", esp_get_free_heap_size() - esp_get_minimum_free_heap_size());
        // we divide by 512KB because the heap has that size instead of 4MB
        sensor_data[2] = (esp_get_free_heap_size() - esp_get_minimum_free_heap_size()) / 512;

        // Sending for each sensor metric the message value to the topic

        for (size_t i = 0; i < sensor_length; i++) {
            asprintf(&sensor_message, " {\"sensor_type\": \"%s\", \"sensor_value\": %ld }", sensor_metrics[i], sensor_data[i]);
            char *message = create_mqtt_message(sensor_message);

            ESP_LOGI(MESH_TAG, "Trying to queue message: %s", message);
            if (mqtt_queues->mqttPublisherQueue != NULL) {
                publish(sensor_topic[i], message);
                ESP_LOGI(MESH_TAG, "queued done: %s", message);
            }
            free(message);
            free(sensor_message);
        }

        //////// CONFIG - DO NOT TOUCH THIS
            vTaskDelay(pdMS_TO_TICKS(config->polling_time));
            free(config);
        //////// CONFIG - END
    }
    for (size_t i = 0; i < sensor_length; i++) {
        free(sensor_topic[i]);
    }
    vTaskDelete(NULL);
}