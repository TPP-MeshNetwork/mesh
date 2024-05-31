#include "../utils/sensor_utils.h"
// Sensor: DH11
#include <components/dht.h>
#define SENSOR_TYPE DHT_TYPE_DHT11
#define DHT11_GPIO 33

void task_sensor_dh11(void *args) {
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *) args;
    char *sensor_message;

    const int max_tries = 10;
    int tries = 0;

    const char *sensor_name[DHT11_SENSOR_COUNT] = {"temperature", "humidity"};

    char * sensor_topic[DHT11_SENSOR_COUNT] = {
        create_topic("sensor", "temperature", true),
        create_topic("sensor", "humidity", true)
    };

    size_t sensor_length = sizeof(sensor_name) / sizeof(sensor_name[0]);

    bool mocked = true;
    float sensor_data[DHT11_SENSOR_COUNT] = {20.0f, 75.0f}; // initial values

    while (1) {
        if (mocked) {
            
            mockDh11SensorData(sensor_data, sensor_name);

            ESP_LOGI(MESH_TAG, "%s: %.1fC\n", sensor_name[0], sensor_data[0]);
        }
        else if (dht_read_float_data(SENSOR_TYPE, DHT11_GPIO, sensor_data + 1, sensor_data) == ESP_OK)
        {
            ESP_LOGI(MESH_TAG, "%s: %.1fC\n", sensor_name[0], sensor_data[0]);
        }
        else {
            // stopping reading sensor if it fails too many times
            tries++;
            if (tries > max_tries)
                break;
            ESP_LOGI(MESH_TAG, "Could not read data from sensor\n");
            vTaskDelay(pdMS_TO_TICKS(1000));
            continue;
        }

        for (size_t i = 0; i < sensor_length; i++) {
            asprintf(&sensor_message, " \"sensor_type\": \"%s\", \"sensor_value\": %.1f ", sensor_name[i], sensor_data[i]);
            char *message = create_message(sensor_message);

            ESP_LOGI(MESH_TAG, "Trying to queue message: %s", message);
            if (mqtt_queues->mqttPublisherQueue != NULL) {
                publish(sensor_topic[i], message);
                ESP_LOGI(MESH_TAG, "queued done: %s", message);
            }
            free(message);
            free(sensor_message);
        }

        vTaskDelay(pdMS_TO_TICKS(10000));
    }
    free(sensor_topic[0]);
    free(sensor_topic[1]);
    vTaskDelete(NULL);
}