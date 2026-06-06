// Adapt-Cloudy/hal/hal_timer.h
// 定时器硬件抽象层
// 封装 esp_timer，提供单次/周期定时器

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 定时器句柄
typedef void *hal_timer_handle_t;

// 定时器回调函数类型
typedef void (*hal_timer_cb_t)(void *arg);

/**
 * @brief 创建单次定时器
 * @param name      定时器名称
 * @param timeout_ms 超时时间（毫秒）
 * @param callback  回调函数
 * @param arg       回调参数
 * @return 定时器句柄，失败返回 NULL
 */
hal_timer_handle_t hal_timer_create_once(const char *name, uint32_t timeout_ms,
                                          hal_timer_cb_t callback, void *arg);

/**
 * @brief 创建周期定时器
 * @param name      定时器名称
 * @param period_ms 周期时间（毫秒）
 * @param callback  回调函数
 * @param arg       回调参数
 * @return 定时器句柄，失败返回 NULL
 */
hal_timer_handle_t hal_timer_create_periodic(const char *name, uint32_t period_ms,
                                              hal_timer_cb_t callback, void *arg);

/**
 * @brief 启动单次定时器
 * @param handle     定时器句柄
 * @param timeout_ms 超时时间（毫秒）
 * @return true=成功, false=失败
 */
bool hal_timer_start_once(hal_timer_handle_t handle, uint32_t timeout_ms);

/**
 * @brief 启动周期定时器
 * @param handle   定时器句柄
 * @param period_ms 周期时间（毫秒）
 * @return true=成功, false=失败
 */
bool hal_timer_start_periodic(hal_timer_handle_t handle, uint32_t period_ms);

/**
 * @brief 停止定时器
 * @param handle  定时器句柄
 * @return true=成功, false=失败
 */
bool hal_timer_stop(hal_timer_handle_t handle);

/**
 * @brief 删除定时器
 * @param handle  定时器句柄
 */
void hal_timer_delete(hal_timer_handle_t handle);

/**
 * @brief 获取系统启动后的毫秒数
 * @return 毫秒数
 */
int64_t hal_timer_get_ms(void);

#ifdef __cplusplus
}
#endif
