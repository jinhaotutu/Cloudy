// Adapt-Cloudy/hal/hal_gpio.c
// GPIO 硬件抽象层实现

#include "hal_gpio.h"
#include "driver/gpio.h"
#include "esp_log.h"

static const char *TAG = "hal_gpio";

void hal_gpio_init(int gpio_num, hal_gpio_dir_t direction, hal_gpio_pull_t pull)
{
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << gpio_num),
        .mode = (direction == HAL_GPIO_DIR_OUTPUT) ? GPIO_MODE_OUTPUT : GPIO_MODE_INPUT,
        .pull_up_en = (pull == HAL_GPIO_PULL_UP) ? GPIO_PULLUP_ENABLE : GPIO_PULLUP_DISABLE,
        .pull_down_en = (pull == HAL_GPIO_PULL_DOWN) ? GPIO_PULLDOWN_ENABLE : GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };

    esp_err_t ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "gpio_config failed for GPIO%d: %s", gpio_num, esp_err_to_name(ret));
    }
}

void hal_gpio_set_level(int gpio_num, bool level)
{
    gpio_set_level(gpio_num, level ? 1 : 0);
}

bool hal_gpio_get_level(int gpio_num)
{
    return gpio_get_level(gpio_num) ? true : false;
}
