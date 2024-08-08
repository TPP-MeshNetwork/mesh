#ifndef STATUS_LED_H
#define STATUS_LED_H

#include "esp_log.h"
#include <freertos/FreeRTOS.h>
#include "driver/gpio.h"
#include "provisioning.h"
#include "persistence.h"


/* RGB LED PIns*/
#define GPIO_RED_PIN 12
#define GPIO_GREEN_PIN 14
#define GPIO_BLUE_PIN 27


void init_status_led();
void test_status_led();
void set_low();
void status_led_set_red();
void status_led_set_green();
void status_led_set_blue();

//esp_err_t init_config_button(void);
//void check_pin_status(void *arg);

#endif // STATUS_LED_H