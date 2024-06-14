#ifndef RESET_BUTTON_H
#define RESET_BUTTON_H

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include "driver/gpio.h"
#include "provisioning.h"
#include "persistence.h"

#define GPIO_NETWORK_MANAGER_CONFIGURED 2
/* Reset button*/
#define GPIO_RESET_BUTTON 13


typedef struct led_config_control {
    unsigned long run_time;
    int cancel; // if cancel is 1 then the task will be deleted
} led_config_control_t;

void init_config_led();
void set_config_led(bool status);
void blink_config_led(void * args);

esp_err_t init_config_button(void);
void check_pin_status(void *arg);

#endif // RESET_BUTTON_H