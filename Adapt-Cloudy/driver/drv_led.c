// Adapt-Cloudy/driver/drv_led.c
// LED 指示灯驱动实现

#include "drv_led.h"
#include "hal_gpio.h"
#include "pin_config.h"

static bool s_led_state = false;

void drv_led_init(void)
{
    hal_gpio_init(LED_GPIO, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE);
    hal_gpio_set_level(LED_GPIO, false);
    s_led_state = false;
}

void drv_led_on(void)
{
    hal_gpio_set_level(LED_GPIO, true);
    s_led_state = true;
}

void drv_led_off(void)
{
    hal_gpio_set_level(LED_GPIO, false);
    s_led_state = false;
}

void drv_led_toggle(void)
{
    s_led_state = !s_led_state;
    hal_gpio_set_level(LED_GPIO, s_led_state);
}

bool drv_led_get_state(void)
{
    return s_led_state;
}
