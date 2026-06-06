// Adapt-Cloudy/driver/drv_matrix_key.h
// 3×3 矩阵键盘扫描驱动
// 10ms 扫描周期，100ms 软件去抖
// 通过 FreeRTOS 队列发送按键事件

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"

#ifdef __cplusplus
extern "C" {
#endif

// 按键编号（对应矩阵位置）
typedef enum {
    KEY_ID_NONE = 0,
    KEY_ID_K1 = 1,   // 乳制品 (ROW0, COL0)
    KEY_ID_K2 = 2,   // 肉蛋类 (ROW0, COL1)
    KEY_ID_K3 = 3,   // 蔬菜类 (ROW0, COL2)
    KEY_ID_K4 = 4,   // 水果类 (ROW1, COL0)
    KEY_ID_K5 = 5,   // 海鲜类 (ROW1, COL1)
    KEY_ID_K6 = 6,   // 饮品类 (ROW1, COL2)
    KEY_ID_K7 = 7,   // 冷冻类 (ROW2, COL0)
    KEY_ID_K8 = 8,   // 确认键 (ROW2, COL2)
    KEY_ID_MAX = 9,
} key_id_t;

// 按键事件类型
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,         // 按键按下（去抖后）
    KEY_EVENT_RELEASE,       // 按键释放
    KEY_EVENT_SHORT_PRESS,   // 短按 (< 5s，松开时触发)
    KEY_EVENT_LONG_PRESS,    // 长按 (>= 5s，松开时触发)
} key_event_type_t;

// 按键事件结构体
typedef struct {
    key_id_t         key_id;
    key_event_type_t event;
    uint32_t         duration_ms;  // 按住时长（毫秒）
} key_event_t;

/**
 * @brief 初始化矩阵键盘 GPIO
 */
void drv_matrix_key_init(void);

/**
 * @brief 启动键盘扫描任务
 * @param event_queue  按键事件输出队列
 */
void drv_matrix_key_start(QueueHandle_t event_queue);

/**
 * @brief 获取按键名称字符串
 * @param key_id  按键编号
 * @return 按键名称
 */
const char *drv_matrix_key_get_name(key_id_t key_id);

#ifdef __cplusplus
}
#endif
