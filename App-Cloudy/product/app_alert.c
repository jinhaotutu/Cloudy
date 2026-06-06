// App-Cloudy/product/app_alert.c
// 过期提醒模块实现 — LED状态指示
// 原型阶段使用单色LED（GPIO21），通过闪烁模式区分状态

#include "app_alert.h"
#include "drv_led.h"
#include "svc_time.h"
#include "esp_log.h"

static const char *TAG = "app_alert";

// 闪烁参数
#define BLINK_INTERVAL_SLOW_MS  1000    // 蓝色慢闪周期
#define BLINK_INTERVAL_FAST_MS  300     // 红色快闪周期

// 当前状态
static led_state_t s_current_state = LED_STATE_OFF;
static int64_t s_last_blink_time = 0;
static bool s_blink_on = false;

void app_alert_init(void)
{
    ESP_LOGI(TAG, "Alert module init");
    drv_led_init();
    s_current_state = LED_STATE_OFF;
    s_blink_on = false;
    drv_led_off();
}

void app_alert_update(bool has_expiring, bool has_expired)
{
    led_state_t new_state;

    if (has_expired) {
        new_state = LED_STATE_RED;
    } else if (has_expiring) {
        new_state = LED_STATE_YELLOW;
    } else {
        new_state = LED_STATE_GREEN;
    }

    if (new_state != s_current_state) {
        ESP_LOGI(TAG, "LED state: %d -> %d", s_current_state, new_state);
        app_alert_set_led(new_state);
    }
}

void app_alert_set_led(led_state_t state)
{
    s_current_state = state;
    s_last_blink_time = svc_time_get_uptime_ms();
    s_blink_on = false;

    switch (state) {
    case LED_STATE_OFF:
        drv_led_off();
        break;
    case LED_STATE_GREEN:
        drv_led_on();
        break;
    case LED_STATE_YELLOW:
        drv_led_on();
        break;
    case LED_STATE_RED:
        drv_led_off();  // 快闪由 blink_handler 控制
        break;
    case LED_STATE_BLUE_BLINK:
        drv_led_off();  // 慢闪由 blink_handler 控制
        break;
    }
}

void app_alert_blink_handler(void)
{
    int64_t now = svc_time_get_uptime_ms();
    int64_t interval;

    switch (s_current_state) {
    case LED_STATE_RED:
        interval = BLINK_INTERVAL_FAST_MS;
        break;
    case LED_STATE_BLUE_BLINK:
        interval = BLINK_INTERVAL_SLOW_MS;
        break;
    default:
        return;  // 非闪烁状态，直接返回
    }

    if (now - s_last_blink_time >= interval) {
        s_blink_on = !s_blink_on;
        if (s_blink_on) {
            drv_led_on();
        } else {
            drv_led_off();
        }
        s_last_blink_time = now;
    }
}

led_state_t app_alert_get_state(void)
{
    return s_current_state;
}
