// Adapt-Cloudy/hal/hal_gpio.h
// GPIO 硬件抽象层
// 封装 ESP-IDF gpio_hal，提供统一接口

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// GPIO 方向
typedef enum {
    HAL_GPIO_DIR_INPUT = 0,
    HAL_GPIO_DIR_OUTPUT,
} hal_gpio_dir_t;

// GPIO 上拉/下拉
typedef enum {
    HAL_GPIO_PULL_NONE = 0,
    HAL_GPIO_PULL_UP,
    HAL_GPIO_PULL_DOWN,
} hal_gpio_pull_t;

/**
 * @brief 初始化 GPIO 引脚
 * @param gpio_num  GPIO 编号
 * @param direction 方向（输入/输出）
 * @param pull      上拉/下拉配置
 */
void hal_gpio_init(int gpio_num, hal_gpio_dir_t direction, hal_gpio_pull_t pull);

/**
 * @brief 设置 GPIO 输出电平
 * @param gpio_num  GPIO 编号
 * @param level     true=高电平, false=低电平
 */
void hal_gpio_set_level(int gpio_num, bool level);

/**
 * @brief 读取 GPIO 输入电平
 * @param gpio_num  GPIO 编号
 * @return true=高电平, false=低电平
 */
bool hal_gpio_get_level(int gpio_num);

#ifdef __cplusplus
}
#endif
