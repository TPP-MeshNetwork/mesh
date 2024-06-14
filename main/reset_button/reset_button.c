#include "./reset_button.h"

// Global variables to track the button state
bool last_status_is_pressed = false;

extern char *MESH_TAG;

void init_config_led() {
    gpio_reset_pin(GPIO_NETWORK_MANAGER_CONFIGURED);
    gpio_set_direction(GPIO_NETWORK_MANAGER_CONFIGURED, GPIO_MODE_OUTPUT);
    gpio_set_level(GPIO_NETWORK_MANAGER_CONFIGURED, 0);
}

void set_config_led(bool status) {
    gpio_set_level(GPIO_NETWORK_MANAGER_CONFIGURED, status);
}

void blink_config_led(void * args) {
    led_config_control_t * led_ctrl = (led_config_control_t*) args;
    int elapsed_time = 0;
    int delay_time = 200; // Initial delay time in ms

    while (elapsed_time < led_ctrl->run_time * 1000) {
        if (led_ctrl->cancel) break;
        gpio_set_level(GPIO_NETWORK_MANAGER_CONFIGURED, 1);
        vTaskDelay(pdMS_TO_TICKS(delay_time));
        gpio_set_level(GPIO_NETWORK_MANAGER_CONFIGURED, 0);
        vTaskDelay(pdMS_TO_TICKS(delay_time));

        // Increase elapsed time
        elapsed_time += 2 * delay_time;

        delay_time = delay_time > 10 ? delay_time - 10 : 10; // Ensure delay time does not go below 10 ms
    }
    // Turn off the LED after the run time
    gpio_set_level(GPIO_NETWORK_MANAGER_CONFIGURED, 0);
    vTaskDelete(NULL);
}

void check_pin_status(void *arg) {
    ESP_LOGI(MESH_TAG, "[check_pin_status] Initializing button check");
    unsigned long pressed_start_time = 0;
    const unsigned long press_duration = 5; // Number of seconds the button should be pressed to trigger restart

    led_config_control_t *led_ctrl = (led_config_control_t *) malloc(sizeof(led_config_control_t));
    led_ctrl->run_time = press_duration;
    led_ctrl->cancel = 0;

    while (1) {
        bool pressed = !(bool) gpio_get_level(GPIO_RESET_BUTTON);
        unsigned long now = xTaskGetTickCount() / configTICK_RATE_HZ;

        if (pressed) {
            if (!last_status_is_pressed) {
                // Button was just pressed
                ESP_LOGI(MESH_TAG, "[check_pin_status] >>> Button press detected <<<");
                pressed_start_time = now;
                xTaskCreate(blink_config_led, "blink_config_led", 1024, (void *) led_ctrl, 5, NULL);
            } else if (now - pressed_start_time >= press_duration) {
                // Button has been pressed for `press_duration` seconds
                ESP_LOGI(MESH_TAG, "[check_pin_status] >>> Button press duration met <<<");
                ESP_LOGI(MESH_TAG, "[check_pin_status] Erasing persistence and restarting the device");
                led_ctrl->cancel = 1;
                persistence_erase_namespace(NETWORK_MANAGER_PERSISTENCE_NAMESPACE);
                esp_restart();
            }
        } else {
            if (last_status_is_pressed) {
                // Button was just released
                ESP_LOGI(MESH_TAG, "[check_pin_status] >>> Button release detected <<<");
                led_ctrl->cancel = 1;
            }
            // Reset the pressed start time if button is released
            pressed_start_time = 0;
        }

        last_status_is_pressed = pressed;
        vTaskDelay(20 / portTICK_PERIOD_MS);
    }

    vTaskDelete(NULL);
}

esp_err_t init_config_button(void) {
    gpio_reset_pin(GPIO_RESET_BUTTON);
    gpio_set_direction(GPIO_RESET_BUTTON, GPIO_MODE_INPUT);
    return ESP_OK;
}