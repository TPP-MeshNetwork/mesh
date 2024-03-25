/*
   This example code is in the Public Domain (or CC0 licensed, at your option.)

   Unless required by applicable law or agreed to in writing, this
   software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
   CONDITIONS OF ANY KIND, either express or implied.
*/
#pragma once

#include "stdbool.h"
#include "esp_err.h"

typedef void (ConfigCallback)(char* ssid, uint8_t channel, char* password, char* mesh_name, char* email);

esp_err_t app_wifi_init(void);
esp_err_t app_wifi_start(ConfigCallback* callback);