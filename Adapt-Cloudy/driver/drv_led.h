// Adapt-Cloudy/driver/drv_led.h
// LED 指示灯驱动
// 控制 XIAO 板载 LED（GPIO21），高电平点亮

#pragma once

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 LED GPIO
 */
void drv_led_init(void);

/**
 * @brief 点亮 LED
 */
void drv_led_on(void);

/**
 * @brief 熄灭 LED
 */
void drv_led_off(void);

/**
 * @brief 切换 LED 状态
 */
void drv_led_toggle(void);

/**
 * @brief 获取 LED 当前状态
 * @return true=亮, false=灭
 */
bool drv_led_get_state(void);

#ifdef __cplusplus
}
#endif
