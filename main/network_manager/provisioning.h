#include <ctype.h>
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
#include "mdns.h"
#include "stdbool.h"
#include "esp_err.h"
#include "mbedtls/base64.h"

#define UNCONFIGURED_FLAG 0
#define CONFIGURED_FLAG 1

#define NETWORK_MANAGER_PERSISTENCE_NAMESPACE "network_manager"

typedef void (ConfigCallback)(char* ssid, uint8_t channel, char* password, char* mesh_name, char* email);

esp_err_t app_wifi_init(void);
esp_err_t app_wifi_start(ConfigCallback* callback);

/* network_manager_callback
*  Description: Callback function to be called when the network manager has the credentials
*/
void network_manager_callback(char *ssid, uint8_t channel, char *password, char *mesh_name, char *email);

char * base64_encode(const char *input);