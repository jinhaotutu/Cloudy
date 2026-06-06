// App-Cloudy/product/app_ui.h
// UI 状态机模块 — 屏幕页面管理
// 参考：Q2_设备端交互规格.md 第7节状态机

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "drv_matrix_key.h"

#ifdef __cplusplus
extern "C" {
#endif

// UI 页面状态
typedef enum {
    UI_PAGE_HOME = 0,       // 首页（空闲状态）
    UI_PAGE_RECORD,         // 录入确认页
    UI_PAGE_LIST,           // 食材列表页
    UI_PAGE_ALERT,          // 过期告警页
    UI_PAGE_CONFIG,         // 配网模式页
} ui_page_t;

// UI 事件类型
typedef enum {
    UI_EVENT_NONE = 0,
    UI_EVENT_KEY_PRESS,         // 按键按下
    UI_EVENT_TIMEOUT,           // 超时
    UI_EVENT_EXPIRY_CHECK,      // 过期检查完成
} ui_event_type_t;

// UI 事件结构体
typedef struct {
    ui_event_type_t type;
    union {
        key_event_t key;        // 按键事件
        uint32_t timeout_ms;    // 超时时长
    };
} ui_event_t;

/**
 * @brief 初始化 UI 状态机
 */
void app_ui_init(void);

/**
 * @brief 获取当前页面状态
 * @return 当前页面
 */
ui_page_t app_ui_get_current_page(void);

/**
 * @brief 处理按键事件
 * @param event 按键事件
 */
void app_ui_handle_key(const key_event_t *event);

/**
 * @brief 处理超时事件
 */
void app_ui_handle_timeout(void);

/**
 * @brief 处理过期检查事件
 * @param has_expiring 有临期食材
 * @param has_expired 有过期食材
 */
void app_ui_handle_expiry_check(bool has_expiring, bool has_expired);

/**
 * @brief UI 主循环处理（10ms调用一次）
 *        检查超时、刷新屏幕
 */
void app_ui_update(void);

/**
 * @brief 刷新当前页面显示
 */
void app_ui_refresh(void);

#ifdef __cplusplus
}
#endif
