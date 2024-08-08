#include "./status_led.h"


void init_status_led() {
    gpio_reset_pin(GPIO_RED_PIN);
    gpio_reset_pin(GPIO_GREEN_PIN);
    gpio_reset_pin(GPIO_BLUE_PIN);
    gpio_set_direction(GPIO_RED_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_GREEN_PIN, GPIO_MODE_OUTPUT);
    gpio_set_direction(GPIO_BLUE_PIN, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_RED_PIN, 0);
    gpio_set_level(GPIO_GREEN_PIN, 0);
    gpio_set_level(GPIO_BLUE_PIN, 0);
}

void set_low() {
    gpio_set_level(GPIO_RED_PIN, 0);
    gpio_set_level(GPIO_GREEN_PIN, 0);
    gpio_set_level(GPIO_BLUE_PIN, 0);
}

void status_led_set_red() {
    set_low();
    gpio_set_level(GPIO_RED_PIN, 1);
}

void status_led_set_green() {
    set_low();
    gpio_set_level(GPIO_GREEN_PIN, 1);
}

void status_led_set_blue() {
    set_low();
    gpio_set_level(GPIO_BLUE_PIN, 1);
}

void test_status_led() {
    while (true) {
        // Test Red
        set_low();
        gpio_set_level(GPIO_RED_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        // Test Green
        set_low();
        gpio_set_level(GPIO_GREEN_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
        // Test Blue
        set_low();
        gpio_set_level(GPIO_BLUE_PIN, 1);
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
