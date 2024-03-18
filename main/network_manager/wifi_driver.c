#include <stdio.h>
#include <string.h>
#include <esp_log.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/event_groups.h>
#include <esp_wifi.h>
#include <esp_event.h>
#include <esp_netif.h>
#include <esp_http_server.h>
#include "cJSON.h"

#define MIN(a, b) ((a) < (b) ? (a) : (b))

static const char *TAG = "app_wifi";

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;
typedef void (ConfigCallback)(char* ssid, char* password, char* mesh_name, char* email);

extern const uint8_t provisioning_html_start[] asm("_binary_provisioning_html_start");
extern const uint8_t provisioning_html_end[] asm("_binary_provisioning_html_end");

typedef struct
{
    uint16_t count;
    char **ssids;
} wifi_scan_result_t;

// Define a structure to hold extra arguments for the handler
typedef struct {
    int luckyNumber;
    ConfigCallback* callback;
} handler_args_t;


typedef struct {
    httpd_handle_t server;
    void* callback;
} server_custom_arg_t;


static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == WIFI_EVENT_STA_START)
    {
        esp_wifi_connect();
    }
    else if (event_id == WIFI_EVENT_STA_DISCONNECTED)
    {
        ESP_LOGI(TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect();
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        /* Signal main application to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ESP_LOGI(TAG, "+++++++++++++++++++++EVENTO++++++++++++ %ld %s", event_id, event_base);
    if (event_base == WIFI_EVENT)
    {
        wifi_event_handler(arg, event_base, event_id, event_data);
    }
    else if (event_base == IP_EVENT)
    {
        ip_event_handler(arg, event_base, event_id, event_data);
    }
}

void scan_wifi_networks()
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL, // Scan for all SSIDs
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true // Include hidden networks
    };
    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
}

static wifi_scan_result_t scan_networks()
{
    wifi_scan_config_t scan_config = {
        .ssid = NULL,
        .bssid = NULL,
        .channel = 0,
        .show_hidden = true};

    wifi_scan_result_t result;
    memset(&result, 0, sizeof(wifi_scan_result_t));

    ESP_ERROR_CHECK(esp_wifi_scan_start(&scan_config, true));
    esp_wifi_scan_start(&scan_config, true);
    esp_wifi_scan_get_ap_num(&(result.count));

    result.ssids = malloc(result.count * sizeof(char *));
    if (result.ssids == NULL)
    {
        ESP_LOGE("scan_wifi_networks", "Failed to allocate memory for SSIDs");
        return result;
    }

    // Retrieve SSIDs and dynamically allocate memory for each SSID
    wifi_ap_record_t *ap_records = malloc(result.count * sizeof(wifi_ap_record_t));
    if (ap_records == NULL)
    {
        ESP_LOGE("scan_wifi_networks", "Failed to allocate memory for AP records");
        free(result.ssids);
        return result;
    }
    esp_wifi_scan_get_ap_records(&(result.count), ap_records);
    for (int i = 0; i < result.count; i++)
    {
        result.ssids[i] = strdup((char *)ap_records[i].ssid);
    }
    free(ap_records); // Free memory for AP records

    return result;
}

static esp_err_t scan_networks_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "application/json");
    wifi_scan_result_t scan_result = scan_networks();
    cJSON *root = cJSON_CreateObject();
    cJSON *ssid_array = cJSON_CreateArray();

    for (int i = 0; i < scan_result.count; i++)
    {
        cJSON_AddItemToArray(ssid_array, cJSON_CreateString(scan_result.ssids[i]));
    }
    cJSON_AddItemToObject(root, "ssids", ssid_array);

    char *json_str = cJSON_Print(root);

    httpd_resp_sendstr(req, json_str);

    cJSON_Delete(root);
    free(json_str);

    return ESP_OK;
}

static esp_err_t hello_get_handler(httpd_req_t *req)
{
    httpd_resp_set_type(req, "text/html");
    httpd_resp_send(req, (const char *)provisioning_html_start, provisioning_html_end - provisioning_html_start);
    return ESP_OK;
}

static esp_err_t provision_post_handler(httpd_req_t *req)
{
    char *buf = NULL;
    size_t buf_len = req->content_len;
    int ret;
    ESP_LOGI(TAG, "Lucky Number: %d", ((handler_args_t *)req->user_ctx)->luckyNumber);
    handler_args_t *args = (handler_args_t *)req->user_ctx;

    buf = malloc(buf_len + 1);
    if (buf == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for request body");
        return ESP_FAIL;
    }

    if ((ret = httpd_req_recv(req, buf, buf_len)) <= 0)
    {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT)
        {
            return ESP_OK;
        }
        free(buf);
        ESP_LOGE(TAG, "Failed to receive request body");
        return ESP_FAIL;
    }

    buf[buf_len] = '\0';

    char *wifi_ssid = NULL, *wifi_password = NULL, *mesh_name = NULL, *email = NULL;

    wifi_ssid = malloc(strlen(buf) + 1);
    wifi_password = malloc(strlen(buf) + 1);
    mesh_name = malloc(strlen(buf) + 1);
    email = malloc(strlen(buf) + 1);

    if (wifi_ssid == NULL || wifi_password == NULL || mesh_name == NULL || email == NULL)
    {
        ESP_LOGE(TAG, "Failed to allocate memory for SSID and password");
        free(wifi_ssid);
        free(wifi_password);
        free(mesh_name);
        free(email);
        return ESP_FAIL;
    }

    if (httpd_query_key_value(buf, "wifi_ssid", wifi_ssid, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "wifi_password", wifi_password, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "mesh_name", mesh_name, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "email", email, strlen(buf) + 1) == ESP_OK)
    {
        ESP_LOGI(TAG, "Received Wi-Fi SSID: %s, Password: %s, Mesh Name: %s, Email: %s", wifi_ssid, wifi_password, mesh_name, email);
        const char *response = "Form submitted successfully!";
        httpd_resp_send(req, response, strlen(response));

        // Call Callback if exists
        // TODO
        if (args->callback != NULL) {
            args->callback(wifi_ssid, wifi_password, mesh_name, email);
        }
        
        
        free(wifi_ssid);
        free(wifi_password);
        free(mesh_name);
        free(email);
        free(buf);
        return ESP_OK;

        // Logica de aprovisionamiento
    }

    httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid or missing form data");
    free(wifi_ssid);
    free(wifi_password);
    free(mesh_name);
    free(email);
    free(buf);
    return ESP_FAIL;
}

handler_args_t extra_args = {
    .luckyNumber = 77,

};

static const httpd_uri_t hello = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = hello_get_handler,
    .user_ctx = NULL};

static const httpd_uri_t scan = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_networks_handler,
    .user_ctx = NULL};

static httpd_uri_t provision = {
    .uri = "/provision",
    .method = HTTP_POST,
    .handler = provision_post_handler,
    .user_ctx = NULL};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err)
{
    if (strcmp("/hello", req->uri) == 0)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    }
    else if (strcmp("/echo", req->uri) == 0)
    {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

static httpd_handle_t start_webserver(void* callback)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    extra_args.callback = callback;

    provision.user_ctx = &extra_args;

    ESP_LOGI(TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK)
    {
        ESP_LOGI(TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &provision);
        return server;
    }
    ESP_LOGI(TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server)
{
    httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server)
    {
        ESP_LOGI(TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data)
{
    server_custom_arg_t *server_custom_arg = (server_custom_arg_t *)arg;
    httpd_handle_t *server = (httpd_handle_t *)server_custom_arg->server;
    if (*server == NULL)
    {
        ESP_LOGI(TAG, "Starting webserver");
        *server = start_webserver(server_custom_arg->callback);
    }
}

esp_err_t app_wifi_init(void)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    wifi_config_t conf = {
        .ap = {
            .ssid = "admin-milos",
            .ssid_len = strlen("admin-milos"),
            .password = "admin-milos",
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK},
    };

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &conf));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t app_wifi_start(ConfigCallback* callback)
{
    static httpd_handle_t server = NULL;
    server_custom_arg_t server_custom_arg = {
        .server = NULL,
        .callback = callback
    };
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &connect_handler, &server_custom_arg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &disconnect_handler, &server));
    server_custom_arg.server = start_webserver(callback);

    return ESP_OK;
}