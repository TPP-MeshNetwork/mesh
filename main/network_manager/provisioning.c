
#include "provisioning.h"
#include "../persistence/persistence.h"


#define MIN(a, b) ((a) < (b) ? (a) : (b))

extern char * MESH_TAG;

const int WIFI_CONNECTED_EVENT = BIT0;
static EventGroupHandle_t wifi_event_group;
typedef void (ConfigCallback)(char* ssid, uint8_t channel, char* password, char* mesh_name, char* email);

extern const uint8_t provisioning_html_start[] asm("_binary_provisioning_html_start");
extern const uint8_t provisioning_html_end[] asm("_binary_provisioning_html_end");

typedef struct {
    uint16_t count;
    char **ssids;
    uint8_t* channels;
} wifi_scan_result_t;

// Define a structure to hold extra arguments for the handler
typedef struct {
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
        ESP_LOGI(MESH_TAG, "Disconnected. Connecting to the AP again...");
        esp_wifi_connect();
    }
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    if (event_id == IP_EVENT_STA_GOT_IP)
    {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(MESH_TAG, "Connected with IP Address:" IPSTR, IP2STR(&event->ip_info.ip));
        /* Signal main application to continue execution */
        xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_EVENT);
    }
}

/* Event handler for catching system events */
static void event_handler(void *arg, esp_event_base_t event_base,
                          int32_t event_id, void *event_data)
{
    ESP_LOGI(MESH_TAG, "+++++++++++++++++++++ EVENT ++++++++++++ %ld %s", event_id, event_base);
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
    result.channels = malloc(result.count * sizeof(int));

    if (result.ssids == NULL || result.channels == NULL)
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
        free(result.channels);
        return result;
    }
    esp_wifi_scan_get_ap_records(&(result.count), ap_records);
    for (int i = 0; i < result.count; i++)
    {
        result.ssids[i] = strdup((char *)ap_records[i].ssid);
        result.channels[i] = ap_records[i].primary;

        ESP_LOGI("scan_wifi_networks", "Found SSID: %s, Channel: %d", result.ssids[i], result.channels[i]);
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
    cJSON *channel_array = cJSON_CreateArray();

    for (int i = 0; i < scan_result.count; i++)
    {
        cJSON_AddItemToArray(ssid_array, cJSON_CreateString(scan_result.ssids[i]));
        cJSON_AddItemToArray(channel_array, cJSON_CreateNumber(scan_result.channels[i]));
    }
    cJSON_AddItemToObject(root, "ssids", ssid_array);
    cJSON_AddItemToObject(root, "channels", channel_array);

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


char* url_decode(char* input) {
    ESP_LOGI(MESH_TAG, "URL DECODE: %s", input);
    char* output = malloc(strlen(input) + 1);
    int i = 0, j = 0;
    if (output == NULL) {
        ESP_LOGE(MESH_TAG, "Failed to allocate memory for URL Decoding");
        return NULL;
    }
    while (input[i]) {
        char aux = input[i+1];
        char aux2 = input[i+2];
        if (input[i] == '%' && isxdigit(aux) && isxdigit(aux2)) {
            char hex_str[3];
            hex_str[0] = input[i + 1];
            hex_str[1] = input[i + 2];
            hex_str[2] = '\0';
            int hex_value;
            sscanf(hex_str, "%x", &hex_value);
            output[j++] = hex_value;
            i += 3;
        } else if (input[i] == '+') {
            output[j++] = ' ';
            i++;
        } else {
            output[j++] = input[i++];
        }
    }
    output[j] = '\0';
    //ESP_LOGI(MESH_TAG, "URL DECODED: %s", output);
    return output;
}


static esp_err_t provision_post_handler(httpd_req_t *req) {
    char *buf = NULL;
    size_t buf_len = req->content_len;
    int ret;
    handler_args_t *args = (handler_args_t *)req->user_ctx;

    buf = malloc(buf_len + 1);
    if (buf == NULL) {
        ESP_LOGE(MESH_TAG, "Failed to allocate memory for request body");
        return ESP_FAIL;
    }

    if ((ret = httpd_req_recv(req, buf, buf_len)) <= 0) {
        if (ret == HTTPD_SOCK_ERR_TIMEOUT) {
            return ESP_OK;
        }
        free(buf);
        ESP_LOGE(MESH_TAG, "Failed to receive request body");
        return ESP_FAIL;
    }

    buf[buf_len] = '\0';

    char *wifi_ssid = NULL, *wifi_password = NULL, *mesh_name = NULL, *email = NULL, *wifi_channel = NULL;

    wifi_ssid = malloc(strlen(buf) + 1);
    wifi_password = malloc(strlen(buf) + 1);
    mesh_name = malloc(strlen(buf) + 1);
    email = malloc(strlen(buf) + 1);
    wifi_channel = malloc(strlen(buf) + 1);

    if (wifi_ssid == NULL || wifi_password == NULL || mesh_name == NULL || email == NULL || wifi_channel == NULL) {
        ESP_LOGE(MESH_TAG, "Failed to allocate memory for SSID and password");
        free(wifi_ssid);
        free(wifi_password);
        free(mesh_name);
        free(email);
        free(wifi_channel);
        return ESP_FAIL;
    }

    if (httpd_query_key_value(buf, "wifi_ssid", wifi_ssid, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "wifi_channel", wifi_channel, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "wifi_password", wifi_password, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "mesh_name", mesh_name, strlen(buf) + 1) == ESP_OK &&
        httpd_query_key_value(buf, "email", email, strlen(buf) + 1) == ESP_OK) {
        ESP_LOGI(MESH_TAG, "Received Wi-Fi SSID: %s, Channel: %s, Password: %s, Mesh Name: %s, Email: %s", wifi_ssid, wifi_channel, wifi_password, mesh_name, email);
        
        const char *response = "Form submitted successfully!";
        httpd_resp_send(req, response, strlen(response));

        // Call Callback if exists

        char* decoded_widi_ssid = url_decode(wifi_ssid);
        char* decoded_wifi_password = url_decode(wifi_password);
        char* decoded_mesh_name = url_decode(mesh_name);
        char* decoded_email = url_decode(email);

        free(wifi_ssid);
        free(wifi_password);
        free(mesh_name);
        free(email);
        free(buf);

        ESP_LOGI(MESH_TAG, "Channel value is :%s", wifi_channel);
        ESP_LOGI(MESH_TAG, "Channel value is :%d", atoi(wifi_channel));

        if (args->callback != NULL) {
            ESP_LOGI(MESH_TAG, "About to call callback");
            args->callback(decoded_widi_ssid, (uint8_t) atoi(wifi_channel), decoded_wifi_password, decoded_mesh_name, decoded_email);
        } else {
            ESP_LOGE(MESH_TAG, "Callback is NULL");
        }
        
        free(wifi_channel);

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

static const httpd_uri_t hello = {
    .uri = "/",
    .method = HTTP_GET,
    .handler = hello_get_handler,
    .user_ctx = NULL
};

static const httpd_uri_t scan = {
    .uri = "/scan",
    .method = HTTP_GET,
    .handler = scan_networks_handler,
    .user_ctx = NULL
};

static httpd_uri_t provision = {
    .uri = "/provision",
    .method = HTTP_POST,
    .handler = provision_post_handler,
    .user_ctx = NULL
};

esp_err_t http_404_error_handler(httpd_req_t *req, httpd_err_code_t err) {
    if (strcmp("/hello", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/hello URI is not available");
        /* Return ESP_OK to keep underlying socket open */
        return ESP_OK;
    }
    else if (strcmp("/echo", req->uri) == 0) {
        httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "/echo URI is not available");
        /* Return ESP_FAIL to close underlying socket */
        return ESP_FAIL;
    }
    /* For any other URI send 404 and close socket */
    httpd_resp_send_err(req, HTTPD_404_NOT_FOUND, "Some 404 error message");
    return ESP_FAIL;
}

handler_args_t extra_args = {
        .callback = NULL
    };


static httpd_handle_t start_webserver(void* callback) {
    ESP_ERROR_CHECK( mdns_init() );
    ESP_ERROR_CHECK( mdns_hostname_set("milos") );
    ESP_LOGI(MESH_TAG, "mdns hostname set to: [%s]", "milos.local");
    ESP_ERROR_CHECK( mdns_instance_name_set("Milos network provisioning") );
    ESP_ERROR_CHECK(mdns_service_add(NULL, "_http", "_tcp", 80, NULL, 0));

    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.lru_purge_enable = true;

    extra_args.callback = callback;

    provision.user_ctx = &extra_args;

    ESP_LOGI(MESH_TAG, "Starting server on port: '%d'", config.server_port);
    if (httpd_start(&server, &config) == ESP_OK) {
        ESP_LOGI(MESH_TAG, "Registering URI handlers");
        httpd_register_uri_handler(server, &hello);
        httpd_register_uri_handler(server, &scan);
        httpd_register_uri_handler(server, &provision);
        return server;
    }
    ESP_LOGI(MESH_TAG, "Error starting server!");
    return NULL;
}

static void stop_webserver(httpd_handle_t server) {
    httpd_stop(server);
}

static void disconnect_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
    httpd_handle_t *server = (httpd_handle_t *)arg;
    if (*server) {
        ESP_LOGI(MESH_TAG, "Stopping webserver");
        stop_webserver(*server);
        *server = NULL;
    }
}

static void connect_handler(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
    server_custom_arg_t *server_custom_arg = (server_custom_arg_t *)arg;
    httpd_handle_t *server = (httpd_handle_t *)server_custom_arg->server;
    if (*server == NULL) {
        ESP_LOGI(MESH_TAG, "Starting webserver");
        *server = start_webserver(server_custom_arg->callback);
    }
}

esp_err_t app_wifi_init(void) {
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_ap();
    wifi_event_group = xEventGroupCreate();

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &event_handler, NULL));
    // ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &event_handler, NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // Creating the SSID
    char * macSta = get_mac_sta();
    // slice the last 6 characters of the mac address
    char * last6macSta = macSta + strlen(macSta) - 6;
    // create ssid as uint8_t ssid[32] adding the last 6 characters of the mac address
    char prefix[] = "MILOS:";
    char * ssid;
    ssid = malloc(strlen(prefix) + 6 + 1);
    if (ssid == NULL) {
        ESP_LOGE(MESH_TAG, "Failed to allocate memory for SSID");
        return ESP_FAIL;
    }
    snprintf(ssid, strlen(prefix) + strlen(last6macSta) + 1, "%s%s", prefix, last6macSta);
    unsigned char unsigned_ssid[32];
    for (int i = 0; i < 32; i++) {
        unsigned_ssid[i] = (unsigned char) ssid[i];
    }
    unsigned_ssid[32] = '\0';
    ESP_LOGI("Network Manager", "SSID: %s\n", unsigned_ssid);

    wifi_config_t conf = {
        .ap = {
            .ssid_len = strlen((char *)unsigned_ssid),
            .max_connection = 4,
            .authmode = WIFI_AUTH_OPEN
        },
    };
    memcpy(conf.ap.ssid, unsigned_ssid, conf.ap.ssid_len);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_APSTA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_AP, &conf));
    ESP_ERROR_CHECK(esp_wifi_start());

    return ESP_OK;
}

esp_err_t app_wifi_start(ConfigCallback* callback) {
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

char * base64_encode(const char *input) {
    size_t input_len = strlen(input);
    size_t olen = 0; // output length
    size_t output_len = 4 * ((strlen(input) + 2) / 3) + 1;
    char * output = malloc(sizeof(char) * output_len);

    int ret = mbedtls_base64_encode((unsigned char *)output, output_len, &olen, (const unsigned char *)input, input_len);
    if (ret != 0) {
        ESP_LOGE(MESH_TAG, "Failed to encode base64: %d", ret);
        return NULL;
    }

    output[olen] = '\0'; // Null-terminate the output string
    return output;
}

void network_manager_callback(char *ssid, uint8_t channel, char *password, char *mesh_name, char *email) {
    ESP_LOGI(MESH_TAG, "[network_manager_callback] called");
    // TODO: hide password from logs 
    ESP_LOGI(MESH_TAG, "[network_manager_callback] Received config Wi-Fi SSID: %s, Channel: %d, Password: ******, Mesh Name: %s, Email: %s", ssid, channel, mesh_name, email);
    persistence_handler_t handler = persistence_open(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
    persistence_set_str(handler, "ssid", ssid);
    persistence_set_str(handler, "password", password);
    persistence_set_u8(handler, "channel", channel);
    persistence_set_u8(handler, "configured", CONFIGURED_FLAG);
    // set the mesh_tag with the mesh_name and base64(email)
    // char *delimiter = "__";
    // char * unique_mail_encode = base64_encode(email);
    // if (unique_mail_encode == NULL) {
    //     ESP_LOGE(MESH_TAG, "Failed to encode email to base64");
    //     // reset the device
    //     persistence_erase_namespace(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
    //     esp_restart();
    // }
    // slice in half the unique_mail_encode
    // unique_mail_encode[strlen(unique_mail_encode)/2] = '\0';
    // size_t mesh_tag_len = strlen(mesh_name) + strlen(delimiter) + strlen(unique_mail_encode)/2 + 1;
    // char * mesh_tag = malloc(sizeof(char) * mesh_tag_len);
    // snprintf(mesh_tag, mesh_tag_len, "%s%s%s", mesh_name, delimiter, unique_mail_encode);
    // ESP_LOGI(MESH_TAG, "Mesh Tag: %s", mesh_tag);
    // persistence_set_str(handler, "MESH_TAG", mesh_tag);
    persistence_set_str(handler, "MESH_TAG", mesh_name);

    persistence_set_str(handler, "EMAIL", email);
    esp_restart();
}
