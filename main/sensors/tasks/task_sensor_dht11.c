#include "../utils/sensor_utils.h"
#include "tasks_config.h"
// Sensor: DH11
#include <components/dht.h>
#define SENSOR_TYPE DHT_TYPE_DHT11
#define DHT11_GPIO 33
#define DHT11_SENSOR_METRIC_COUNT 2

void task_sensor_dht11(void *args) {
    TaskJobArgs_t * args_ = (TaskJobArgs_t *)args;
    mqtt_queues_t *mqtt_queues = args_->mqtt_queues;
    int job_id = args_->id;
    char *sensor_message;

    const int max_tries = 10;
    int tries = 0;

    // Getting metrics from task_id configured
    char ** sensor_metrics = get_sensor_metrics_by_task_id(job_id);

    for (size_t i = 0; sensor_metrics[i] != NULL; i++) {
        ESP_LOGI(MESH_TAG, "Sensor metrics: %s", sensor_metrics[i]);
    }

    // Getting the sensor metrics count
    size_t sensor_length = get_sensor_count(sensor_metrics);

    // Creating the response topics from the metrics
    char * sensor_topic[sensor_length];
    for (size_t i = 0; i < sensor_length; i++) {
        sensor_topic[i] = create_topic("sensor", sensor_metrics[i], true);
    }

    // If sensor should be mocked
    bool mocked = true;
    float sensor_data[] = {20.0f, 75.0f};

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
        if (mocked) {
            mockDh11SensorData(sensor_data, (const char **) sensor_metrics);
            // ESP_LOGI(MESH_TAG, "%s: %.1fC\n", sensor_metrics[0], sensor_data[0]);
        }
        else if (dht_read_float_data(SENSOR_TYPE, DHT11_GPIO, sensor_data + 1, sensor_data) == ESP_OK) {
            // ESP_LOGI(MESH_TAG, "%s: %.1fC\n", sensor_metrics[0], sensor_data[0]);
        }
        else {
            // stopping reading sensor if it fails too many times
            tries++;
            if (tries > max_tries)
                break;
            ESP_LOGI(MESH_TAG, "Could not read data from sensor\n");
            //////// CONFIG - DO NOT TOUCH THIS
                vTaskDelay(pdMS_TO_TICKS(config->polling_time));
                free(config);
            //////// CONFIG - END
            continue;
        }

        for (size_t i = 0; i < sensor_length; i++) {
            asprintf(&sensor_message, " \"sensor_type\": \"%s\", \"sensor_value\": %.1f ", sensor_metrics[i], sensor_data[i]);
            char *message = create_message(sensor_message);

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
    free(sensor_topic[0]);
    free(sensor_topic[1]);
    vTaskDelete(NULL);
}