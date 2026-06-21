// App-Cloudy/product/app_ui.h
// UI 状态机模块 — 屏幕页面管理

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "drv_matrix_key.h"

#ifdef __cplusplus
extern "C" {
#endif

// UI 页面状态
typedef enum {
    UI_PAGE_HOME = 0,       // 首页（食材网格）
    UI_PAGE_RECORD,         // 录入确认页
    UI_PAGE_CONFIG,         // 配网模式页
} ui_page_t;

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
 * @brief UI 主循环处理（10ms调用一次）
 */
void app_ui_update(void);

/**
 * @brief 刷新当前页面显示
 */
void app_ui_refresh(void);

#ifdef __cplusplus
}
#endif
