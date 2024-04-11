/* Mesh Internal Communication Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "esp_wifi.h"
#include "esp_mac.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_mesh.h"
#include "nvs_flash.h"
#include "mesh_netif.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include <components/dht.h>
#include "mqtt/aws_mqtt.h"
#include "mqtt_queue.h"
#include "network_transport.h"
#include "esp_netif_sntp.h"
#include <freertos/FreeRTOS.h>
#include "esp_system.h"
#include "network_manager/provisioning.h"
#include "persistence/persistence.h"

/*******************************************************
 *                Macros
 *******************************************************/
#define CMD_ROUTE_TABLE 0x56

#define CONFIG_EXAMPLE_DATA_GPIO 33
#define SENSOR_TYPE DHT_TYPE_DHT11
/*******************************************************
 *                Constants
 *******************************************************/
#define RST_BTN 13
#define UNCONFIGURED_FLAG 0
#define CONFIGURED_FLAG 1
static char * MESH_TAG = "esp32-mesh";
static char * EMAIL = "";
// MESH ID must be a 6-byte array to identify the mesh network and its created from the first 6 bytes of the MESH_TAG
static uint8_t MESH_ID[6] = { 0x65, 0x73, 0x70, 0x33, 0x32, 0x2D};

/*******************************************************
 *                Variable Definitions for Mesh
 *******************************************************/
static bool is_running = true;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static esp_ip4_addr_t s_current_ip;
static mesh_addr_t s_route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
static int s_route_table_size = 0;
static SemaphoreHandle_t s_route_table_lock = NULL;
static uint8_t s_mesh_tx_payload[CONFIG_MESH_ROUTE_TABLE_SIZE * 6 + 1];
/*For button pressing*/
static unsigned long int last_time = 0;
static bool last_status_is_pressed = true;


void publish(QueueHandle_t publishQueue, const char *topic, const char *message) {
    mqtt_message_t mqtt_message;
    strcpy(mqtt_message.topic, topic);
    strcpy(mqtt_message.message, message);
    xQueueSend(publishQueue, &mqtt_message, 0);
}

void static recv_cb(mesh_addr_t *from, mesh_data_t *data) {
    if (data->data[0] == CMD_ROUTE_TABLE)
    {
        int size = data->size - 1;
        if (s_route_table_lock == NULL || size % 6 != 0) {
            ESP_LOGE(MESH_TAG, "Error in receiving raw mesh data: Unexpected size");
            return;
        }
        xSemaphoreTake(s_route_table_lock, portMAX_DELAY);
        s_route_table_size = size / 6;
        for (int i = 0; i < s_route_table_size; ++i) {
            ESP_LOGI(MESH_TAG, "Received Routing table [%d] " MACSTR, i, MAC2STR(data->data + 6 * i + 1));
        }
        memcpy(&s_route_table, data->data + 1, size);
        xSemaphoreGive(s_route_table_lock);
    }
    else
    {
        ESP_LOGE(MESH_TAG, "Error in receiving raw mesh data: Unknown command");
    }
}

void log_perfdata() {
    uint32_t free_heap_size = 0, min_free_heap_size = 0;
    free_heap_size = esp_get_free_heap_size();
    min_free_heap_size = esp_get_minimum_free_heap_size();
    printf("\n\n free heap size = %ld \t  min_free_heap_size = %ld \n\n", free_heap_size, min_free_heap_size);
}

char * create_topic(char* topic_type, char* topic_suffix, bool withDeviceIndicator) {
    if (topic_type == NULL || topic_suffix == NULL) {
        ESP_LOGE(MESH_TAG, "Error in create_topic: topic_type or topic_suffix is NULL");
        return NULL;
    }
    uint8_t macAp[6];
    esp_wifi_get_mac(WIFI_IF_AP, macAp);
    char *topic;
    if (withDeviceIndicator) {
        asprintf(&topic, "/mesh/%s/device/" MACSTR "/%s/%s", MESH_TAG, MAC2STR(macAp), topic_type, topic_suffix);
    } else {
        asprintf(&topic, "/mesh/%s/%s/%s", MESH_TAG, topic_type, topic_suffix);
    }
    return topic;
}

char *create_message(char *message) {
    if (message == NULL)
    {
        ESP_LOGE(MESH_TAG, "Error in create_message message is NULL");
        return NULL;
    }

    // get the timestamp for the message in epoch
    time_t now;
    time(&now);

    uint8_t macAp[6];
    esp_wifi_get_mac(WIFI_IF_AP, macAp);
    char * new_message;
    asprintf(&new_message, "{\"mesh_id\": \"%s\", \"device_id\": \"" MACSTR "\", \"timestamp_value\": %lld, %s }", MESH_TAG, MAC2STR(macAp), now, message);
    return new_message;
}

void task_read_sensor_dh11(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_read_sensor_dh11");
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *)args;
    char *sensor_message;
    const int max_tries = 10;
    int tries = 0;

    const char *sensor_name[2] = {"temperature", "humidity"};

    char * sensor_topic[2] = {
        create_topic("sensor", "temperature", true),
        create_topic("sensor", "humidity", true)
    };

    size_t sensor_length = sizeof(sensor_name) / sizeof(sensor_name[0]);
    float sensor_data[sensor_length];

    bool mocked = true;

    while (1)
    {
        if (mocked)
        {
            int min = 15;
            int max = 25;
            sensor_data[0] = (float)(rand() % (max - min + 1) + min);

            min = 50;
            max = 100;
            sensor_data[1] = (float)(rand() % (max - min + 1) + min);

            ESP_LOGI(MESH_TAG, "%s: %.1fC\n", sensor_name[0], sensor_data[0]);
        }
        else if (dht_read_float_data(SENSOR_TYPE, CONFIG_EXAMPLE_DATA_GPIO, sensor_data + 1, sensor_data) == ESP_OK)
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
            if (mqtt_queues->mqttPublisherQueue != NULL)
            {
                publish(mqtt_queues->mqttPublisherQueue, sensor_topic[i], message);
                ESP_LOGI(MESH_TAG, "queued done: %s", message);
            }
            free(message);
            free(sensor_message);
        }

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    free(sensor_topic[0]);
    free(sensor_topic[1]);
    vTaskDelete(NULL);
}

void task_mesh_table_routing(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_mesh_table_routing");
    is_running = true;
    mesh_data_t data;
    esp_err_t err;
    while (is_running) {
        if (esp_mesh_is_root()) {
            esp_mesh_get_routing_table((mesh_addr_t *)&s_route_table,
                                       CONFIG_MESH_ROUTE_TABLE_SIZE * 6, &s_route_table_size);
            data.size = s_route_table_size * 6 + 1;
            data.proto = MESH_PROTO_BIN;
            data.tos = MESH_TOS_P2P;
            s_mesh_tx_payload[0] = CMD_ROUTE_TABLE;
            memcpy(s_mesh_tx_payload + 1, s_route_table, s_route_table_size * 6);
            data.data = s_mesh_tx_payload;
            for (int i = 0; i < s_route_table_size; i++) {
                err = esp_mesh_send(&s_route_table[i], &data, MESH_DATA_P2P, NULL, 0);
                ESP_LOGI(MESH_TAG, "Sending routing table to [%d] " MACSTR ": sent with err code: %d", i, MAC2STR(s_route_table[i].addr), err);
            }
        }
        vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void concatenateSensorNames(const char *sensor_name[], int size, char result[]) {
    for (int i = 0; i < size; i++) {
        strcat(result, "\"");
        strcat(result, sensor_name[i]);
        strcat(result, "\", ");
    }

    // Remove the trailing comma and space
    if (strlen(result) >= 2) {
        result[strlen(result) - 2] = '\0';
    }
}

void task_notify_new_device_id(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_notify_new_device_id");
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *) args;
    char *device_id_msg;

    char * device_topic = create_topic("devices", "report", false);
    const char *sensor_name[2] = {"temperature", "humidity"};
    char result[100] = ""; // Initialize result string

    while (1) {
        uint8_t macAp[6];
        esp_wifi_get_mac(WIFI_IF_AP, macAp);
        concatenateSensorNames(sensor_name, sizeof(sensor_name) / sizeof(sensor_name[0]), result);
        asprintf(&device_id_msg, "{\"mesh_id\": \"%s\", \"device_id\": \"" MACSTR "\", \"sensor_metrics\": [%s]}", MESH_TAG, MAC2STR(macAp), result);

        ESP_LOGI(MESH_TAG, "Trying to queue message: %s", device_id_msg);
        if (mqtt_queues->mqttPublisherQueue != NULL) {
            publish(mqtt_queues->mqttPublisherQueue, device_topic, device_id_msg);
            ESP_LOGI(MESH_TAG, "queued done: %s - %s", device_topic, device_id_msg);
        }
        free(device_id_msg);
        vTaskDelay(24 * 3600 * 1000 / portTICK_PERIOD_MS);
    }
    free(device_topic);
    vTaskDelete(NULL);
}

void task_notify_new_user_connected(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_notify_new_mesh");
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *) args;
    char *device_id_msg;

    char * device_topic = create_topic("email", "report", false);

    for (int i = 0; i < 5; i++) {
        if (esp_mesh_is_root()) {
            uint8_t macAp[6];
            esp_wifi_get_mac(WIFI_IF_AP, macAp);
            asprintf(&device_id_msg, "{\"mesh_id\": \"%s\", \"email\": \"%s\"}", MESH_TAG, EMAIL);

            ESP_LOGI(MESH_TAG, "Trying to queue message: %s", device_id_msg);
            if (mqtt_queues->mqttPublisherQueue != NULL) {
                publish(mqtt_queues->mqttPublisherQueue, device_topic, device_id_msg);
                ESP_LOGI(MESH_TAG, "queued done: %s - %s", device_topic, device_id_msg);
            }
            free(device_id_msg);
            vTaskDelay(24 * 3600 * 1000 / portTICK_PERIOD_MS);
        }
    }
    free(device_topic);
    vTaskDelete(NULL);
}


void task_mqtt_graph(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_mqtt_graph");

    is_running = true;
    char *graph_message;
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *)args;
    // get the parent of this node
    mesh_addr_t parent;
    esp_mesh_get_parent_bssid(&parent);

    // mac addr STA of this node to string
    uint8_t macSta[6];
    esp_wifi_get_mac(WIFI_IF_STA, macSta);

    // mac addr AP of this node to string
    uint8_t macAp[6];
    esp_wifi_get_mac(WIFI_IF_AP, macAp);

    char * topic = create_topic("graph", "report", false);

    while (is_running) {
        log_perfdata(); // for debugging memory leaks

        if (esp_mesh_is_root()) {
            // the root node has no parent so instead we get the WIFI_IF_AP -> WIFI_IF_STA
            asprintf(&graph_message, "\"layer\": %d, \"root\": true, \"macSta\": \"" MACSTR "\", \"macSoftap\": \"" MACSTR "\"", esp_mesh_get_layer(), MAC2STR(macSta), MAC2STR(macAp));
        }
        else
        {
            asprintf(&graph_message, "\"layer\": %d, \"root\": false, \"macSta\": \"" MACSTR "\", \"macSoftap\": \"" MACSTR "\"", esp_mesh_get_layer(), MAC2STR(parent.addr), MAC2STR(macAp));
        }

        char *message = create_message(graph_message);

        ESP_LOGI(MESH_TAG, "Trying to queue message: %s", message);
        if (mqtt_queues->mqttPublisherQueue != NULL)
        {
            publish(mqtt_queues->mqttPublisherQueue, topic, message);
            ESP_LOGI(MESH_TAG, "queued done: %s", message);
        }
        free(graph_message);
        free(message);
        vTaskDelay(pdMS_TO_TICKS(2000));
    }
    free(topic);
    vTaskDelete(NULL);
}

// void esp_mesh_task_mqtt_keepalive(void *arg)
// {
//     is_running = true;
//     char *print;
//     uint8_t macList[2][6];

//     // mac addr of this node to string
//     esp_wifi_get_mac(WIFI_IF_AP, macList[0]);
//     // get the parent of this node
//     mesh_addr_t parent;
//     esp_mesh_get_parent_bssid(&parent);
//     memcpy(macList[1], parent.addr, 6);

//     size_t macListLength = sizeof(macList) / sizeof(macList[0]);
//     while (is_running)
//     {

//         for (size_t i = 0; i < macListLength; i++)
//         {
//             // send keepalive this mac
//             asprintf(&print, "{'macSta': '" MACSTR "'}", MAC2STR(macList[i]));
//             ESP_LOGI(MESH_TAG, "Tried to publish topic: keepalive %s", print);
//             mqtt_app_publish("/topic/keepalive", MESH_TAG, print);
//             free(print);
//         }

//         vTaskDelay(2 * 2000 / portTICK_PERIOD_MS);
//     }
//     vTaskDelete(NULL);
// }

void task_mqtt_client_start(void *args) {
    // read mqtt queues from arg
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *)args;

    MQTTContext_t mqttContext = {0};
    NetworkContext_t xNetworkContext = {0};

    ESP_LOGI(MESH_TAG, "STARTED: task_mqtt_client_start");

    int mqtt_connection_status = start_mqtt_connection(&mqttContext, &xNetworkContext);
    while (1)
    {

        while (mqtt_connection_status == EXIT_FAILURE)
        {
            mqtt_connection_status = start_mqtt_connection(&mqttContext, &xNetworkContext);
            vTaskDelay(1000 / portTICK_PERIOD_MS);
        }
        // read queue for messages to publish
        mqtt_message_t *buffer = malloc(sizeof(mqtt_message_t));
        if (buffer == NULL)
        {
            // Handle allocation failure
            ESP_LOGE(MESH_TAG, "Failed to allocate memory for buffer");
        }
        else
        {

            if (mqtt_queues->mqttPublisherQueue != NULL)
            {
                if (xQueueReceive(mqtt_queues->mqttPublisherQueue, (void *)buffer, 0) == pdTRUE)
                {
                    ESP_LOGI(MESH_TAG, "Received message to publish: %s on topic: %s", buffer->message, buffer->topic);
                    int returnStatus = publishToTopic(&mqttContext, buffer->message, buffer->topic, MQTTQoS0);
                    if (returnStatus != EXIT_SUCCESS)
                    {
                        ESP_LOGI(MESH_TAG, "Error in publishLoop");
                        disconnectMqttSession(&mqttContext);
                        mqtt_connection_status = EXIT_FAILURE;
                    }
                }
            }
        }
        free(buffer);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

esp_err_t esp_tasks_runner(void) {
    static bool is_comm_mqtt_task_started = false;

    s_route_table_lock = xSemaphoreCreateMutex();

    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *)malloc(sizeof(mqtt_queues_t));

    mqtt_queues->mqttPublisherQueue = xQueueCreate(queueSize, sizeof(mqtt_message_t));
    mqtt_queues->mqttSuscriberQueue = xQueueCreate(queueSize, sizeof(mqtt_message_t));

    if (mqtt_queues->mqttPublisherQueue == NULL)
    {
        ESP_LOGI(MESH_TAG, "Error creating the mqttPublisherQueue");
    }
    if (mqtt_queues->mqttSuscriberQueue == NULL)
    {
        ESP_LOGI(MESH_TAG, "Error creating the mqttSuscriberQueue");
    }

    if (!is_comm_mqtt_task_started)
    {
        xTaskCreate(task_mesh_table_routing, "mqtt routing-table", 3072, NULL, 5, NULL);
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        xTaskCreate(task_mqtt_client_start, "mqtt task-aws", 7168, (void *)mqtt_queues, 5, NULL);
        xTaskCreate(task_read_sensor_dh11, "Read sensor data from sensor", 3072, (void *)mqtt_queues, 5, NULL);
        xTaskCreate(task_mqtt_graph, "Graph logging task", 3072, (void *)mqtt_queues, 5, NULL);
        xTaskCreate(task_notify_new_device_id, "Notify new device in mesh", 3072, (void *)mqtt_queues, 5, NULL);
        xTaskCreate(task_notify_new_user_connected, "Notify new user mail connected", 1024, (void *)mqtt_queues, 5, NULL);
        // xTaskCreate(esp_mesh_task_mqtt_keepalive, "Keepalive task", 3072, NULL, 5, NULL);
        is_comm_mqtt_task_started = true;
    }
    return ESP_OK;
}

void mesh_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data) {
    mesh_addr_t id = {
        0,
    };
    static uint8_t last_layer = 0;

    switch (event_id)
    {
    case MESH_EVENT_STARTED:
    {
        esp_mesh_get_id(&id);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_MESH_STARTED>ID:" MACSTR "", MAC2STR(id.addr));
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_STOPPED:
    {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOPPED>");
        mesh_layer = esp_mesh_get_layer();
    }
    break;
    case MESH_EVENT_CHILD_CONNECTED:
    {
        mesh_event_child_connected_t *child_connected = (mesh_event_child_connected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_CONNECTED>aid:%d, " MACSTR "",
                 child_connected->aid,
                 MAC2STR(child_connected->mac));
    }
    break;
    case MESH_EVENT_CHILD_DISCONNECTED:
    {
        mesh_event_child_disconnected_t *child_disconnected = (mesh_event_child_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHILD_DISCONNECTED>aid:%d, " MACSTR "",
                 child_disconnected->aid,
                 MAC2STR(child_disconnected->mac));
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_ADD:
    {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_ADD>add %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_ROUTING_TABLE_REMOVE:
    {
        mesh_event_routing_table_change_t *routing_table = (mesh_event_routing_table_change_t *)event_data;
        ESP_LOGW(MESH_TAG, "<MESH_EVENT_ROUTING_TABLE_REMOVE>remove %d, new:%d",
                 routing_table->rt_size_change,
                 routing_table->rt_size_new);
    }
    break;
    case MESH_EVENT_NO_PARENT_FOUND:
    {
        mesh_event_no_parent_found_t *no_parent = (mesh_event_no_parent_found_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NO_PARENT_FOUND>scan times:%d",
                 no_parent->scan_times);
    }
    /* TODO handler for the failure */
    break;
    case MESH_EVENT_PARENT_CONNECTED:
    {
        mesh_event_connected_t *connected = (mesh_event_connected_t *)event_data;
        esp_mesh_get_id(&id);
        mesh_layer = connected->self_layer;
        memcpy(&mesh_parent_addr.addr, connected->connected.bssid, 6);
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_CONNECTED>layer:%d-->%d, parent:" MACSTR "%s, ID:" MACSTR "",
                 last_layer, mesh_layer, MAC2STR(mesh_parent_addr.addr),
                 esp_mesh_is_root() ? "<ROOT>" : (mesh_layer == 2) ? "<layer2>"
                                                                   : "",
                 MAC2STR(id.addr));
        last_layer = mesh_layer;
        mesh_netifs_start(esp_mesh_is_root());
    }
    break;
    case MESH_EVENT_PARENT_DISCONNECTED:
    {
        mesh_event_disconnected_t *disconnected = (mesh_event_disconnected_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_PARENT_DISCONNECTED>reason:%d",
                 disconnected->reason);
        mesh_layer = esp_mesh_get_layer();
        mesh_netifs_stop();
    }
    break;
    case MESH_EVENT_LAYER_CHANGE:
    {
        mesh_event_layer_change_t *layer_change = (mesh_event_layer_change_t *)event_data;
        mesh_layer = layer_change->new_layer;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_LAYER_CHANGE>layer:%d-->%d%s",
                 last_layer, mesh_layer,
                 esp_mesh_is_root() ? "<ROOT>" : (mesh_layer == 2) ? "<layer2>"
                                                                   : "");
        last_layer = mesh_layer;
    }
    break;
    case MESH_EVENT_ROOT_ADDRESS:
    {
        mesh_event_root_address_t *root_addr = (mesh_event_root_address_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_ADDRESS>root address:" MACSTR "",
                 MAC2STR(root_addr->addr));
    }
    break;
    case MESH_EVENT_VOTE_STARTED:
    {
        mesh_event_vote_started_t *vote_started = (mesh_event_vote_started_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_VOTE_STARTED>attempts:%d, reason:%d, rc_addr:" MACSTR "",
                 vote_started->attempts,
                 vote_started->reason,
                 MAC2STR(vote_started->rc_addr.addr));
    }
    break;
    case MESH_EVENT_VOTE_STOPPED:
    {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_VOTE_STOPPED>");
        break;
    }
    case MESH_EVENT_ROOT_SWITCH_REQ:
    {
        mesh_event_root_switch_req_t *switch_req = (mesh_event_root_switch_req_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_SWITCH_REQ>reason:%d, rc_addr:" MACSTR "",
                 switch_req->reason,
                 MAC2STR(switch_req->rc_addr.addr));
    }
    break;
    case MESH_EVENT_ROOT_SWITCH_ACK:
    {
        /* new root */
        mesh_layer = esp_mesh_get_layer();
        esp_mesh_get_parent_bssid(&mesh_parent_addr);
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_SWITCH_ACK>layer:%d, parent:" MACSTR "", mesh_layer, MAC2STR(mesh_parent_addr.addr));
    }
    break;
    case MESH_EVENT_TODS_STATE:
    {
        mesh_event_toDS_state_t *toDs_state = (mesh_event_toDS_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_TODS_REACHABLE>state:%d", *toDs_state);
    }
    break;
    case MESH_EVENT_ROOT_FIXED:
    {
        mesh_event_root_fixed_t *root_fixed = (mesh_event_root_fixed_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROOT_FIXED>%s",
                 root_fixed->is_fixed ? "fixed" : "not fixed");
    }
    break;
    case MESH_EVENT_ROOT_ASKED_YIELD:
    {
        mesh_event_root_conflict_t *root_conflict = (mesh_event_root_conflict_t *)event_data;
        ESP_LOGI(MESH_TAG,
                 "<MESH_EVENT_ROOT_ASKED_YIELD>" MACSTR ", rssi:%d, capacity:%d",
                 MAC2STR(root_conflict->addr),
                 root_conflict->rssi,
                 root_conflict->capacity);
    }
    break;
    case MESH_EVENT_CHANNEL_SWITCH:
    {
        mesh_event_channel_switch_t *channel_switch = (mesh_event_channel_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_CHANNEL_SWITCH>new channel:%d", channel_switch->channel);
    }
    break;
    case MESH_EVENT_SCAN_DONE:
    {
        mesh_event_scan_done_t *scan_done = (mesh_event_scan_done_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_SCAN_DONE>number:%d",
                 scan_done->number);
    }
    break;
    case MESH_EVENT_NETWORK_STATE:
    {
        mesh_event_network_state_t *network_state = (mesh_event_network_state_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_NETWORK_STATE>is_rootless:%d",
                 network_state->is_rootless);
    }
    break;
    case MESH_EVENT_STOP_RECONNECTION:
    {
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_STOP_RECONNECTION>");
    }
    break;
    case MESH_EVENT_FIND_NETWORK:
    {
        mesh_event_find_network_t *find_network = (mesh_event_find_network_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_FIND_NETWORK>new channel:%d, router BSSID:" MACSTR "",
                 find_network->channel, MAC2STR(find_network->router_bssid));
    }
    break;
    case MESH_EVENT_ROUTER_SWITCH:
    {
        mesh_event_router_switch_t *router_switch = (mesh_event_router_switch_t *)event_data;
        ESP_LOGI(MESH_TAG, "<MESH_EVENT_ROUTER_SWITCH>new router:%s, channel:%d, " MACSTR "",
                 router_switch->ssid, router_switch->channel, MAC2STR(router_switch->bssid));
    }
    break;
    default:
        ESP_LOGI(MESH_TAG, "unknown id:%" PRId32 "", event_id);
        break;
    }
}

void ip_event_handler(void *arg, esp_event_base_t event_base,
                      int32_t event_id, void *event_data) {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    ESP_LOGI(MESH_TAG, "<IP_EVENT_STA_GOT_IP>IP:" IPSTR, IP2STR(&event->ip_info.ip));
    s_current_ip.addr = event->ip_info.ip.addr;
#if !CONFIG_MESH_USE_GLOBAL_DNS_IP
    esp_netif_t *netif = event->esp_netif;
    esp_netif_dns_info_t dns;
    ESP_ERROR_CHECK(esp_netif_get_dns_info(netif, ESP_NETIF_DNS_MAIN, &dns));
    mesh_netif_start_root_ap(esp_mesh_is_root(), dns.ip.u_addr.ip4.addr);
#endif

    /* Before running the tasks we should try to sync with NTP*/
    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG_MULTIPLE(2,
                           ESP_SNTP_SERVER_LIST("time.windows.com", "pool.ntp.org" ) );
    // config.start = true;                       // start the SNTP service explicitly (after connecting)
    // config.server_from_dhcp = true;             // accept the NTP offers from DHCP server
    // config.renew_servers_after_new_IP = true;   // let esp-netif update the configured SNTP server(s) after receiving the DHCP lease
    // config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
    // config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;  // IP event on which we refresh the configuration


    esp_netif_sntp_init(&config);
    if (esp_netif_sntp_sync_wait(pdMS_TO_TICKS(10000)) != ESP_OK)
    {
        printf("Failed to update system time within 10s timeout");
    }
    esp_tasks_runner();
}

void app_start(void) {
    persistence_handler_t handler = persistence_open();
    size_t len_ssid = 32;
    size_t len_passwd = 64;
    size_t len_MESH_TAG = 64;
    size_t len_EMAIL = 64;
    char* ssid = malloc(len_ssid * sizeof(char));
    char* pwd = malloc(len_passwd * sizeof(char));
    MESH_TAG = malloc(len_MESH_TAG * sizeof(char));
    EMAIL = malloc(len_EMAIL * sizeof(char));
    uint8_t channel;
    persistence_get_str(handler, "ssid", ssid, &len_ssid);
    persistence_get_str(handler, "password", pwd, &len_passwd);
    persistence_get_u8(handler, "channel", &channel);
    persistence_get_str(handler, "MESH_TAG", MESH_TAG, &len_MESH_TAG);
    persistence_get_str(handler, "EMAIL", EMAIL, &len_EMAIL);

    ESP_LOGI(MESH_TAG, "SSID: %s, Channel: %d", ssid, channel);

    // copy the first chars converting as uint8 from mesh_tag to MESH_TAG
    for (int i = 0; i < strlen(MESH_TAG); i++){
        MESH_ID[i] = MESH_TAG[i];
    }
    if (strlen(MESH_TAG) < 6) {
        // repeat the last char until reach 6
        for (int i = strlen(MESH_TAG); i < 6; i++) {
            MESH_ID[i] = MESH_TAG[strlen(MESH_TAG)];
        }
    }

    /*  tcpip initialization */
    ESP_ERROR_CHECK(esp_netif_init());
    /*  event initialization */
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    /*  crete network interfaces for mesh (only station instance saved for further manipulation, soft AP instance ignored */
    ESP_ERROR_CHECK(mesh_netifs_init(recv_cb));

    /*  wifi initialization */
    wifi_init_config_t config = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &ip_event_handler, NULL));
    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_FLASH));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
    ESP_ERROR_CHECK(esp_wifi_start());
    /*  mesh initialization */
    ESP_ERROR_CHECK(esp_mesh_init());
    ESP_ERROR_CHECK(esp_event_handler_register(MESH_EVENT, ESP_EVENT_ANY_ID, &mesh_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mesh_set_max_layer(CONFIG_MESH_MAX_LAYER));
    ESP_ERROR_CHECK(esp_mesh_set_vote_percentage(1));
    ESP_ERROR_CHECK(esp_mesh_set_ap_assoc_expire(10));
    mesh_cfg_t cfg = MESH_INIT_CONFIG_DEFAULT();

    /* mesh ID */
    memcpy((uint8_t *) &cfg.mesh_id, MESH_ID, 6);

    /* router */
    cfg.channel = channel;

    cfg.router.ssid_len = strlen(ssid);
    memcpy((uint8_t *)&cfg.router.ssid, ssid, cfg.router.ssid_len);
    memcpy((uint8_t *)&cfg.router.password, pwd, strlen(pwd));

    /* mesh softAP */
    ESP_ERROR_CHECK(esp_mesh_set_ap_authmode(CONFIG_MESH_AP_AUTHMODE));
    cfg.mesh_ap.max_connection = CONFIG_MESH_AP_CONNECTIONS;
    cfg.mesh_ap.nonmesh_max_connection = CONFIG_MESH_NON_MESH_AP_CONNECTIONS;
    memcpy((uint8_t *)&cfg.mesh_ap.password, CONFIG_MESH_AP_PASSWD,
           strlen(CONFIG_MESH_AP_PASSWD));
    ESP_ERROR_CHECK(esp_mesh_set_config(&cfg));
    /* mesh start */
    ESP_ERROR_CHECK(esp_mesh_start());
    ESP_LOGI(MESH_TAG, "mesh starts successfully, heap:%" PRId32 ", %s", esp_get_free_heap_size(),
             esp_mesh_is_root_fixed() ? "root fixed" : "root not fixed");

    free(ssid);
    free(pwd);
}

void check_pin_status() {
    ESP_LOGI(MESH_TAG, "Iniciando el check_pin_status");
    while(1) {
        bool pressed = !(bool) gpio_get_level(RST_BTN);
        unsigned long int now = xTaskGetTickCount() / configTICK_RATE_HZ;

        if (pressed == last_status_is_pressed) {
            // No hubo cambios en el estado no debo hacer nada
        } else {
            if (now - last_time < 1) {
                // Ingoro por que paso poco tiempo
            } else {
                if (!pressed) {
                    // Soltamos el boton
                    if (now - last_time > 5) {
                        ESP_LOGI(MESH_TAG, ">>> DETECTO SOLTADO DEL BOTON <<<");
                        ESP_LOGI(MESH_TAG, ">>> BORRANDO NVS y REINICIANDO <<<");
                        persistence_erase_namespace();
                        esp_restart();
                    }
                } else {
                    // Presionamos el boton
                    ESP_LOGI(MESH_TAG, ">>> DETECTO PRESIONADO DEL BOTON <<<");
                }
                last_time = now;
                last_status_is_pressed = pressed;
            }
        }
        vTaskDelay(20/portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

esp_err_t config_button(void) {
    gpio_reset_pin(RST_BTN);
    gpio_set_direction(RST_BTN, GPIO_MODE_INPUT);

    return ESP_OK;
}

void network_manager_callback(char *ssid, uint8_t channel, char *password, char *mesh_name, char *email) {
    ESP_LOGI(MESH_TAG, "Llamé al callback");
    ESP_LOGI(MESH_TAG, "Received config Wi-Fi SSID: %s, Channel: %d, Password: %s, Mesh Name: %s, Email: %s", ssid, channel, password, mesh_name, email);
    persistence_handler_t handler = persistence_open();
    persistence_set_str(handler, "ssid", ssid);
    persistence_set_str(handler, "password", password);
    persistence_set_u8(handler, "channel", channel);
    persistence_set_u8(handler, "configured", CONFIGURED_FLAG);
    persistence_set_str(handler, "MESH_TAG", mesh_name);
    persistence_set_str(handler, "EMAIL", email);

    esp_restart();
}

void app_main(void) {
    config_button();
    ESP_LOGI(MESH_TAG, "%i", ESP_IDF_VERSION);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(MESH_TAG, "Iniciando el main");
    
    // Load persistence to check if device has already been configured or not
    persistence_err_t persistence_err = persistence_init();
    if (persistence_err == UNABLE_INITIALIZE_PERSISTENCE) {
        ESP_LOGE(MESH_TAG, "Error al inicializar la persistencia");
    } else {
        ESP_LOGI(MESH_TAG, "Inicializado la persistencia");
        //TODO: Check for possible errors and act accordingly
        persistence_handler_t handler = persistence_open();
        uint8_t is_configured;
        persistence_err = persistence_get_u8(handler, "configured", &is_configured);
        if (is_configured == CONFIGURED_FLAG) {
            ESP_LOGI(MESH_TAG, "El dispositivo ya ha sido configurado");

            xTaskCreate(check_pin_status, "button", 3072, NULL,5,NULL );
            // Starting the main application that starts the mesh network
            app_start();
        } else {
            ESP_LOGI(MESH_TAG, "El dispositivo no ha sido configurado");
            ESP_LOGI(MESH_TAG, "Iniciando el webserver");
            app_wifi_init();

            app_wifi_start(&network_manager_callback);
            ESP_LOGI(MESH_TAG, "Finalizado el webserver");
        }
        persistence_close(handler);
    }
}
