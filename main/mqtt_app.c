/* Mesh IP Internal Networking Example

   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "esp_log.h"
#include "esp_system.h"
#include "esp_netif.h"
#include "esp_tls.h"

#include "mqtt_client.h"
#include "esp_mac.h"
#include "nvs_flash.h"
#include "nvs_helper.h"


static const char *TAG = "mesh_mqtt";
static esp_mqtt_client_handle_t s_client = NULL;

static esp_err_t mqtt_event_handler_cb(esp_mqtt_event_handle_t event)
{
    switch (event->event_id) {
        case MQTT_EVENT_CONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_CONNECTED");
            if (esp_mqtt_client_subscribe(s_client, "/topic/ip_mesh/key_pressed", 0) < 0) {
                // Disconnect to retry the subscribe after auto-reconnect timeout
                esp_mqtt_client_disconnect(s_client);
            }
            break;
        case MQTT_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "MQTT_EVENT_DISCONNECTED");
            break;

        case MQTT_EVENT_SUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_SUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_UNSUBSCRIBED:
            ESP_LOGI(TAG, "MQTT_EVENT_UNSUBSCRIBED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_PUBLISHED:
            ESP_LOGI(TAG, "MQTT_EVENT_PUBLISHED, msg_id=%d", event->msg_id);
            break;
        case MQTT_EVENT_DATA:
            ESP_LOGI(TAG, "MQTT_EVENT_DATA");
            ESP_LOGI(TAG, "TOPIC=%.*s", event->topic_len, event->topic);
            ESP_LOGI(TAG, "DATA=%.*s", event->data_len, event->data);
            break;
        case MQTT_EVENT_ERROR:
            ESP_LOGI(TAG, "MQTT_EVENT_ERROR");
            break;
        default:
            ESP_LOGI(TAG, "Other event id:%d", event->event_id);
            break;
    }
    return ESP_OK;
}

static void mqtt_event_handler(void *handler_args, esp_event_base_t base, int32_t event_id, void *event_data) {
    ESP_LOGD(TAG, "Event dispatched from event loop base=%s, event_id=%" PRId32 "", base, event_id);
    mqtt_event_handler_cb(event_data);
}

void mqtt_app_publish(const char* topic, const char* publish_prefix, char *publish_string)
{
    // concatenate prefix and topic
    if(publish_prefix == NULL) {
        publish_prefix = "";
    }
    char * full_message;
    asprintf(&full_message, "%s %s", publish_prefix, publish_string);
    
    if (s_client) {
        int msg_id = esp_mqtt_client_publish(s_client, topic, full_message, 0, 1, 0);
        ESP_LOGI(TAG, "sent publish returned msg_id=%d", msg_id);
    }
    free(full_message);
}

void mqtt_app_start(uint8_t mac[6]) {

    if (mac == NULL) {
        ESP_LOGE(TAG, "MAC address is NULL must provide a valid mac address to start Mqtt");
        return;
    }

    // get certificates from nvs
    nvs_handle handle;
    esp_err_t err = nvs_open("certs", NVS_READONLY, &handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Error opening NVS ('certs'): %s", esp_err_to_name(err));
        // Handle the error accordingly
        // abort the program
        abort();
    }

    char * AWS_ROOT_CERT = nvs_load_value_if_exist(TAG, &handle, "aws_root_cert");
    char * DEVICE_CERT = nvs_load_value_if_exist(TAG, &handle, "device_cert");
    char * DEVICE_PRIVATE = nvs_load_value_if_exist(TAG, &handle, "device_priv_key");
    char * AWS_IOT_ENDPOINT = nvs_load_value_if_exist(TAG, &handle, "aws_iot_host");

    printf("AWS_ROOT_CERT: %s\n", AWS_ROOT_CERT);
    printf("DEVICE_CERT: %s\n", DEVICE_CERT);
    printf("DEVICE_PRIVATE: %s\n", DEVICE_PRIVATE);
    printf("AWS_IOT_ENDPOINT: %s\n", AWS_IOT_ENDPOINT);

    ESP_LOGI(TAG, "AWS_ROOT_CERT size: %d", strlen(AWS_ROOT_CERT));
    ESP_LOGI(TAG, "DEVICE_CERT size: %d", strlen(DEVICE_CERT));
    ESP_LOGI(TAG, "DEVICE_PRIVATE size: %d", strlen(DEVICE_PRIVATE));
    ESP_LOGI(TAG, "AWS_IOT_ENDPOINT: %s", AWS_IOT_ENDPOINT);
    nvs_close(handle);

    // last will topic and message for client disconnection
    char * last_will_topic;
    char * last_will_message;
    asprintf(&last_will_topic, "/topic/keepalive");
    asprintf(&last_will_message, "offline: " MACSTR, MAC2STR(mac));

    // ESP_LOGI(TAG, "Configuring MQTT Broker to %s", CONFIG_MQTT_BROKER_URI);
    esp_mqtt_client_config_t mqtt_cfg = {
        // BROKER CONNECTION
        .broker.address.uri = AWS_IOT_ENDPOINT,
        .broker.address.port = 8883,

        // BROKER CERT VERIFICATION
        .broker.verification.certificate = AWS_ROOT_CERT,
        .broker.verification.skip_cert_common_name_check = true,

        // LAST WILL TOPIC
        // .session.last_will.topic = last_will_topic,
        // .session.last_will.msg = last_will_message,
        // .session.last_will.retain = 0,
        // .session.last_will.qos = 1,
        .session.protocol_ver = MQTT_PROTOCOL_V_3_1_1,
        // .session.keepalive = 30,
        .credentials.authentication.certificate = DEVICE_CERT,
        // .credentials.authentication.certificate_len = strlen(DEVICE_CERT) + 1,
        .credentials.authentication.key = DEVICE_PRIVATE,
        // .credentials.authentication.key_len = strlen(DEVICE_PRIVATE) + 1,
    };

    s_client = esp_mqtt_client_init(&mqtt_cfg);
    esp_mqtt_client_register_event(s_client, ESP_EVENT_ANY_ID, mqtt_event_handler, s_client);
    esp_mqtt_client_start(s_client);
    free(last_will_topic);
    free(last_will_message);
}
