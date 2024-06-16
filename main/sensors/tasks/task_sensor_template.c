#include "../utils/sensor_utils.h"
#include "tasks_config.h"
// Sensor Name: <name>


// This template is for a sensor that has more than one metric (e.g. temperature and humidity)
void task_sensor_template(void *args) {
    TaskJobArgs_t * args_ = (TaskJobArgs_t *)args;
    mqtt_queues_t *mqtt_queues = args_->mqtt_queues;
    int job_id = args_->id;
    char *sensor_message;


    // Getting metrics from task_id configured
    char ** sensor_metrics = get_sensor_metrics_by_task_id(job_id);

    // Getting the sensor metrics count
    size_t sensor_length = get_sensor_count(sensor_metrics);

    // Creating the response topics from the metrics
    char * sensor_topic[sensor_length];
    for (size_t i = 0; i < sensor_length; i++) {
        sensor_topic[i] = create_topic("sensor", sensor_metrics[i], true);
    }

    // For template only (delete this line when implementing the sensor)
    bool sensor_can_read_be_read = true;

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
       
       // Add here the sensor read function 
        if (sensor_can_read_be_read) {
            // sensor_read();
        }
        // If sensor cannot be read (e.g. sensor is not connected)
        else {
            ESP_LOGI(MESH_TAG, "Could not read data from sensor\n");
            //////// CONFIG - DO NOT TOUCH THIS
                vTaskDelay(pdMS_TO_TICKS(config->polling_time));
                free(config);
            //////// CONFIG - END
            continue;
        }

        // Sending for each sensor metric the message value to the topic

        for (size_t i = 0; i < sensor_length; i++) {
            asprintf(&sensor_message, " {\"sensor_type\": \"%s\", \"sensor_value\": %.1f }", sensor_metrics[i], sensor_data[i]);
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