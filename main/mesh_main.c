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
#include "mesh_netif/mesh_netif.h"
#include "driver/gpio.h"
#include "freertos/semphr.h"
#include "mqtt/client/aws_mqtt.h"
#include "network_transport.h"
#include "esp_netif_sntp.h"
#include <freertos/FreeRTOS.h>
#include "network_manager/provisioning.h"
#include "persistence/persistence.h"
#include "suscription_handlers/suscription_event_handlers.h"
#include "mqtt/utils/mqtt_utils.h"
#include "performance/performance.h"
#include "sensors/tasks/sensor_tasks.h"
#include "sensors/utils/sensor_utils.h"
#include "tasks_config/tasks_config.h"
#include "relays/relays.h"
#include "reset_button/reset_button.h"

/*******************************************************
 *                Macros MESH
 *******************************************************/
#define CMD_ROUTE_TABLE 0x56

/*******************************************************
 *                Constants
 *******************************************************/
char * MESH_TAG = "esp32-mesh";
char * SUPPORT_EMAIL = "support@milos.com";
char * USER_EMAIL = NULL;

char * FIRMWARE_VERSION = "v1.0.0";
char * FIRMWARE_REVISION = "rev0.1";

/*******************************************************
 *                Variable Definitions for Mesh
 *******************************************************/
// MESH ID must be a 6-byte array to identify the mesh network and its created from the first 6 bytes of the MESH_TAG
static uint8_t MESH_ID[6] = { 0x65, 0x73, 0x70, 0x33, 0x32, 0x2D};
static bool is_running = true;
static mesh_addr_t mesh_parent_addr;
static int mesh_layer = -1;
static esp_ip4_addr_t s_current_ip;
static mesh_addr_t s_route_table[CONFIG_MESH_ROUTE_TABLE_SIZE];
static int s_route_table_size = 0;
static SemaphoreHandle_t s_route_table_lock = NULL;
static uint8_t s_mesh_tx_payload[CONFIG_MESH_ROUTE_TABLE_SIZE * 6 + 1];

/*******************************************************
 *       Variable Global Definitions for MQTT
 *******************************************************/
char * clientIdentifier = NULL;
mqtt_queues_t *mqtt_queues = NULL;


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
            ESP_LOGD(MESH_TAG, "Received Routing table [%d] " MACSTR, i, MAC2STR(data->data + 6 * i + 1));
        }
        memcpy(&s_route_table, data->data + 1, size);
        xSemaphoreGive(s_route_table_lock);
    }
    else
    {
        ESP_LOGE(MESH_TAG, "Error in receiving raw mesh data: Unknown command");
    }
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
                ESP_LOGD(MESH_TAG, "Sending routing table to [%d] " MACSTR ": sent with err code: %d", i, MAC2STR(s_route_table[i].addr), err);
            }
        }
        vTaskDelay(2 * 1000 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}


char * new_device() {
    char *macAp = get_mac_ap();
    
    cJSON * device_id = cJSON_CreateObject();
    cJSON_AddStringToObject(device_id, "mesh_id", MESH_TAG);
    cJSON_AddStringToObject(device_id, "device_id", macAp);
    cJSON * tasks_array = get_all_tasks_metrics_json();
    cJSON_AddItemToObject(device_id, "tasks", tasks_array);
    // if we have relays we add the relay state
    cJSON * relays_array = get_relay_state();
    cJSON_AddItemToObject(device_id, "relays", relays_array);

    char * device_id_msg = cJSON_Print(device_id);
    return device_id_msg;
}

char * new_user(char *email, size_t count) {
    if (count == 5) {
        return NULL;
    }
    if (esp_mesh_is_root()) {
        char *new_user_message;
        asprintf(&new_user_message, "{\"mesh_id\": \"%s\", \"email\": \"%s\", \"account_role\": \"owner\", \"referer\":  \"%s\"}", MESH_TAG, email, email);
        return new_user_message;
    }
    return NULL;
}

void task_notify_new_device(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_notify_new_device");
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *) args;

    char * new_user_topic = create_topic("usersync", "", false);
    char * device_topic = create_topic("report", "", true);
    size_t new_user_message_sent = 0;
    
    while (1) {
        char * new_user_msg = new_user(USER_EMAIL == NULL? SUPPORT_EMAIL: USER_EMAIL, new_user_message_sent++);
        char * device_msg = new_device();
        if (new_user_msg != NULL) {
            ESP_LOGI(MESH_TAG, "Trying to queue message: %s", new_user_msg);
            if (mqtt_queues->mqttPublisherQueue != NULL) {
                publish(new_user_topic, new_user_msg);
                ESP_LOGI(MESH_TAG, "queued done: %s", new_user_msg);
            }
        }
        if (device_msg != NULL) {
            ESP_LOGI(MESH_TAG, "Trying to queue message: %s", device_msg);
            if (mqtt_queues->mqttPublisherQueue != NULL) {
                publish(device_topic, device_msg);
                ESP_LOGI(MESH_TAG, "queued done: %s", device_msg);
            }
        }
        free(device_msg);
        free(new_user_msg);
        vTaskDelay(24 * 3600 * 1000 / portTICK_PERIOD_MS);
        
        // // every 5 min
        // vTaskDelay(5 * 60 * 1000 / portTICK_PERIOD_MS);
    }
    free(new_user_topic);
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
    char * parent_mac = (char*) malloc(18 * sizeof(char));
    sprintf(parent_mac, MACSTR, MAC2STR(parent.addr));

    // mac addr STA of this node to string
    char * macSta = get_mac_sta();
    // mac addr AP of this node to string
    char * macAp = get_mac_ap();

    char * topic = create_topic("graph", "report", false);

    while (is_running) {
        log_memory(); // for debugging memory leaks

        cJSON * memory_stats = get_memory_stats();

        // create cJSON object with the data
        cJSON *graph_data = cJSON_CreateObject();
        cJSON_AddNumberToObject(graph_data, "layer", esp_mesh_get_layer());

        if (esp_mesh_is_root()) {
            cJSON_AddStringToObject(graph_data, "root", "true");
            cJSON_AddStringToObject(graph_data, "macSta", macSta);
            cJSON_AddStringToObject(graph_data, "macSoftap", macAp);
            // the root node has no parent so instead we get the WIFI_IF_AP -> WIFI_IF_STA
        } else {
            cJSON_AddStringToObject(graph_data, "root", "false");
            cJSON_AddStringToObject(graph_data, "macSta", parent_mac);
            cJSON_AddStringToObject(graph_data, "macSoftap", macAp);
        }

        // Adding perfomance metrics
        cJSON_AddNumberToObject(graph_data, "uptime", get_uptime());
        cJSON_AddItemToObject(graph_data, "memory", memory_stats);

        graph_message = cJSON_PrintUnformatted(graph_data);
        cJSON_Delete(graph_data);

        char *message = create_mqtt_message(graph_message);

        ESP_LOGI(MESH_TAG, "Trying to queue message: %s", message);
        if (mqtt_queues->mqttPublisherQueue != NULL) {
            publish(topic, message);
            ESP_LOGI(MESH_TAG, "queued done: %s", message);
        }
        free(message);
        free(graph_message);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
    free(topic);
    free(parent_mac);
    free(macSta);
    free(macAp);
    vTaskDelete(NULL);
}

void task_mqtt_client_start(void *args) {
    // read mqtt queues from arg
    mqtt_queues_t *mqtt_queues = (mqtt_queues_t *)args;

    MQTTContext_t mqttContext = {0};
    NetworkContext_t xNetworkContext = {0};

    ESP_LOGI(MESH_TAG, "STARTED: task_mqtt_client_start");

    clientIdentifier  = create_client_identifier();

    // get list of topics to subscribe to
    char **topics_list = get_topics_list();


    int mqtt_connection_status = start_mqtt_connection(&mqttContext, &xNetworkContext, clientIdentifier, topics_list);
    while (1) {
        if (mqtt_connection_status == EXIT_FAILURE) {
            ( void ) xTlsDisconnect( &xNetworkContext );
            mqtt_connection_status = start_mqtt_connection(&mqttContext, &xNetworkContext, clientIdentifier, topics_list);
            if (mqtt_connection_status == EXIT_SUCCESS) {
                ESP_LOGE(MESH_TAG, "--- Reconnected to the server");
            }
            vTaskDelay(1000 / portTICK_PERIOD_MS);
            continue;
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
                        mqtt_connection_status = EXIT_FAILURE;
                    }
                }
            }
        }
        /* Calling MQTT_ProcessLoop to process incoming publish. This function also
         * sends ping request to broker if 1 second has expired since the last MQTT packet sent and receive ping responses.
         */
        MQTTStatus_t mqttStatus = processLoopWithTimeout( &mqttContext, 1000 );

        /* For any error in #MQTT_ProcessLoop, exit the loop and disconnect
            * from the broker. */
        if( ( mqttStatus != MQTTSuccess ) && ( mqttStatus != MQTTNeedMoreBytes ) )
        {
            LogError( ( "MQTT_ProcessLoop returned with status = %s.",
                        MQTT_Status_strerror( mqttStatus ) ) );
            
            mqtt_connection_status = EXIT_FAILURE;
        }
        free(buffer);
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    vTaskDelete(NULL);
}

void task_suscriber_event_executor(void *args) {
    // this task only executes the event handler for the suscriber and then finishes the task
    ESP_LOGI(MESH_TAG, "STARTED: task_suscriber_event_executor");
    suscription_event_handler_t *event_handler = (suscription_event_handler_t *)args;
    event_handler->handler(event_handler->topic, event_handler->message);
    free(event_handler->message);
    free(event_handler->topic);
    free(event_handler);
    vTaskDelete(NULL);
}

void task_suscribers_events(void *args) {
    ESP_LOGI(MESH_TAG, "STARTED: task_suscribers_events");
    char **topics_list = get_topics_list();

    while (1) {
        // looping though each topic queue and check if we have an incoming message
        for(size_t i = 0; topics_list[i] != NULL; i++) {
            char * topic = topics_list[i];
            // suscriber_get_message handles also the correct event handler
            char * message = suscriber_get_message(topic);
            SuscriptionTopicsHash_t *s = suscriber_find_topic(topic);
            if (s != NULL) {
                if (message != NULL) {
                    ESP_LOGI(MESH_TAG, "Received message from topic: %s", topic);
                    ESP_LOGI(MESH_TAG, "Message: %s", message);
                    if (s->event_handler != NULL) {
                        // create new suscription_event_handler_t
                        suscription_event_handler_t *event_handler_data = (suscription_event_handler_t *) malloc(sizeof(suscription_event_handler_t));
                        event_handler_data->topic = strdup(topic);
                        // copy the message to the event handler data
                        event_handler_data->message = strdup(message);
                        event_handler_data->handler = s->event_handler;
                        // create a new task to execute the event handler so that this receiver task doesnt block by the handler
                        xTaskCreate(task_suscriber_event_executor, "task_suscriber_event_executor", 5072, (void *)event_handler_data, 5, NULL);
                    }
                    free(message);
                }
            }
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);   
    }
    vTaskDelete(NULL);
}


esp_err_t esp_tasks_runner(void) {
    static bool is_comm_mqtt_task_started = false;

    s_route_table_lock = xSemaphoreCreateMutex();

    mqtt_queues = (mqtt_queues_t *) malloc(sizeof(mqtt_queues_t));
    mqtt_queues->mqttPublisherQueue = xQueueCreate(queueSize, sizeof(mqtt_message_t));
    init_suscriber_hash();
    mqtt_queues->mqttSuscriberHash = suscription_topics;

    if (mqtt_queues->mqttPublisherQueue == NULL)
    {
        ESP_LOGI(MESH_TAG, "Error creating the mqttPublisherQueue");
    }

    /* Adding topics that we want to subscribe to */
    /* Config */
    suscriber_add_topic(create_topic("config", "", false), suscriber_global_config_handler);
    suscriber_add_topic(create_topic("config", "", true), suscriber_particular_config_handler);
    /* Relay Configuration */
    add_relay("Buzzer", GPIO_NUM_22);
    add_relay("Led 1", GPIO_NUM_23);
    relay_init();
    suscriber_add_topic(create_topic("relay", "", true), relay_event_handler);

    if (!is_comm_mqtt_task_started) {
        xTaskCreate(task_mesh_table_routing, "mqtt routing-table", 2048, NULL, 5, NULL);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        xTaskCreate(task_mqtt_client_start, "mqtt task-aws", 8096, (void *)mqtt_queues, 5, NULL);
        xTaskCreate(task_suscribers_events, "Task that reads suscription events", 8096, NULL, 5, NULL);
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);

        char * sensor_dht11_metrics[] = {"temperature", "humidity", NULL};
        create_sensor_task("task_sensor_dht11", "dht11", sensor_dht11_metrics ,task_sensor_dht11, (void *) mqtt_queues, (Config_t) {
            .max_polling_time = 0,  // 0 means no max time restriction
            .min_polling_time = 1000, // 1 second
            .polling_time = 30000, // 30 seconds
            .active = 1 // active
        },
        3072
        );
        char * sensor_performance_metrics[] = {"free_memory", "min_free_memory", "memory_usage", NULL};
        create_sensor_task("task_sensor_performance", "esp32-performance", sensor_performance_metrics ,task_sensor_performance, (void *) mqtt_queues, (Config_t) {
            .max_polling_time = 0,  // 0 means no max time restriction
            .min_polling_time = 5000, // 5 second
            .polling_time = 10000, // 10 seconds
            .active = 1 // active
        },
        3072
        );
        xTaskCreate(task_mqtt_graph, "Graph logging task", 3072, (void *)mqtt_queues, 5, NULL);
        xTaskCreate(task_notify_new_device, "Notify new device", 3072, (void *)mqtt_queues, 5, NULL);
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

    switch (event_id) {
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
        // save channel in nvs
        persistence_handler_t handler = persistence_open(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
        persistence_set_u8(handler, "channel", channel_switch->channel);
        persistence_close(handler);
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
        ESP_LOGE("[task_suscribers_events]", " Failed to update system time within 10s timeout");
        esp_restart();
    }

    // Setting the perfomance uptime value
    set_uptime();

    esp_tasks_runner();
}

void check_wifi_channel(char * ssid, uint8_t * channel) {
    // Start WiFi scan for target SSID channel
    wifi_init_config_t config_scan = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&config_scan));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());
    wifi_scan_config_t scan_config = {
        .ssid = 0,
        .bssid = 0,
        .channel = 0,
        .show_hidden = true
    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));

    // Get scan results
    uint16_t number = 10;
    wifi_ap_record_t ap_info[10];
    uint16_t ap_count = 0;
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_records(&number, ap_info));
    ESP_ERROR_CHECK(esp_wifi_scan_get_ap_num(&ap_count));
    ESP_LOGI(MESH_TAG, "Total APs found: %u", ap_count);

    // Process the scan results
    for (int i = 0; i < ap_count; i++) {
        if (strcmp((char *)ap_info[i].ssid, ssid) == 0) {
            uint8_t current_channel = ap_info[i].primary;
            ESP_LOGI(MESH_TAG, "Found target SSID: %s on channel: %d", ssid, current_channel);
            if (*channel != current_channel) {
                ESP_LOGI(MESH_TAG, "Channel changed from %d to %d", *channel, current_channel);
                persistence_handler_t handler = persistence_open(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
                persistence_set_u8(handler, "channel", current_channel);
                persistence_close(handler);
            }
            *channel = current_channel;
            break;
        }
    }
}

void app_start(void) {
    persistence_handler_t handler = persistence_open(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
    char* ssid = NULL;
    char* pwd = NULL;
    uint8_t channel = 0;
    persistence_get_str(handler, "ssid", &ssid);
    persistence_get_str(handler, "password", &pwd);
    persistence_get_u8(handler, "channel", &channel);
    persistence_get_str(handler, "MESH_TAG", &MESH_TAG);
    persistence_get_str(handler, "EMAIL", &USER_EMAIL);

    if (ssid == NULL || pwd == NULL || USER_EMAIL == NULL) {
        ESP_LOGE(MESH_TAG, "[app_start] Error: Network manager not configured");
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        persistence_erase_namespace(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
        esp_restart();
    }

    ESP_LOGI(MESH_TAG, "SSID: %s, Channel: %d", ssid, channel);

    // Set MESH ID from MESH_TAG
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

    // This function check the active channel and changes it if necessary
    check_wifi_channel(ssid, &channel);    

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

void app_main(void) {
    init_config_button();
    init_config_led();
    ESP_LOGI(MESH_TAG, "%i", ESP_IDF_VERSION);
    vTaskDelay(2000 / portTICK_PERIOD_MS);
    ESP_LOGI(MESH_TAG, "[app_main] main inititalized");
    
    // Load persistence to check if device has already been configured or not
    persistence_err_t persistence_err = persistence_init();
    if (persistence_err == UNABLE_INITIALIZE_PERSISTENCE) {
        ESP_LOGE(MESH_TAG, "[app_main] Error initializing the persistence");
    } else {
        ESP_LOGI(MESH_TAG, "[app_main] Persistence initialized successfully");
        //TODO: Check for possible errors and act accordingly
        persistence_handler_t handler = persistence_open(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
        uint8_t is_configured;
        persistence_err = persistence_get_u8(handler, "configured", &is_configured);
        xTaskCreate(check_pin_status, "button", 3072, NULL,5,NULL );
        if (is_configured == CONFIGURED_FLAG) {
            ESP_LOGI(MESH_TAG, "[app_main] The device has been configured");
            set_config_led(true);
            // Starting the main application that starts the mesh network
            app_start();
        } else {
            ESP_LOGI(MESH_TAG, "[app_main] The device has not been configured yet");
            ESP_LOGI(MESH_TAG, "[app_main] Initializing the webserver");
            app_wifi_init();

            app_wifi_start(&network_manager_callback);
            ESP_LOGI(MESH_TAG, "[app_main] webserver initialized");
        }
        persistence_close(handler);
    }
}
