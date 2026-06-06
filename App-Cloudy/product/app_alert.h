// App-Cloudy/product/app_alert.h
// 过期提醒模块 — LED状态指示
// 参考：Q2_设备端交互规格.md 第8节

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// LED 状态
typedef enum {
    LED_STATE_OFF = 0,      // 灭
    LED_STATE_GREEN,        // 绿色常亮（正常）
    LED_STATE_YELLOW,       // 黄色常亮（临期）
    LED_STATE_RED,          // 红色常亮（过期）
    LED_STATE_BLUE_BLINK,   // 蓝色慢闪（配网）
} led_state_t;

/**
 * @brief 初始化过期提醒模块
 */
void app_alert_init(void);

/**
 * @brief 更新LED状态（根据食材过期情况）
 * @param has_expiring 有临期食材
 * @param has_expired 有过期食材
 */
void app_alert_update(bool has_expiring, bool has_expired);

/**
 * @brief 设置LED状态（强制）
 * @param state LED状态
 */
void app_alert_set_led(led_state_t state);

/**
 * @brief LED闪烁处理（在主循环中调用）
 *        处理蓝色慢闪等需要定时切换的状态
 */
void app_alert_blink_handler(void);

/**
 * @brief 获取当前LED状态
 * @return LED状态
 */
led_state_t app_alert_get_state(void);

#ifdef __cplusplus
}
#endif
