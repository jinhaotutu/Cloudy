// App-Cloudy/product/app_ui.c
// UI 状态机模块实现

#include "app_ui.h"
#include "app_food.h"
#include "drv_st7789.h"
#include "svc_time.h"
#include "esp_log.h"
#include <stdio.h>

static const char *TAG = "app_ui";

// 超时配置（毫秒）
#define TIMEOUT_RECORD_MS       2000    // 录入确认页 2秒
#define TIMEOUT_LIST_MS         30000   // 列表页 30秒
#define TIMEOUT_ALERT_MS        0       // 告警页无自动超时
#define LIST_AUTO_SCROLL_MS     3000    // 列表自动翻页 3秒

// 当前状态
static ui_page_t s_current_page = UI_PAGE_HOME;
static int64_t s_page_enter_time = 0;       // 进入当前页面的时间
static int64_t s_last_refresh_time = 0;     // 上次刷新时间
static int64_t s_last_list_scroll = 0;      // 列表页上次翻页时间

// 录入状态
static uint8_t s_selected_key = 0;          // 当前选中的类别键
static food_record_t s_last_record;         // 最后录入的记录

// 列表状态
static uint32_t s_list_current_index = 0;   // 列表当前显示索引

// 告警状态
static bool s_has_expiring = false;
static bool s_has_expired = false;

// 刷新标志
static bool s_need_refresh = true;

// 前向声明
static void draw_home_page(void);
static void draw_record_page(void);
static void draw_list_page(void);
static void draw_alert_page(void);
static void draw_config_page(void);

void app_ui_init(void)
{
    ESP_LOGI(TAG, "UI init");

    // 清屏
    drv_st7789_fill_screen(COLOR_WHITE);

    // 显示启动信息
    drv_st7789_draw_string_center(60, "Cloudy", COLOR_BLUE, COLOR_WHITE, 3);
    drv_st7789_draw_string_center(120, "Starting...", COLOR_BLACK, COLOR_WHITE, 2);

    s_current_page = UI_PAGE_HOME;
    s_page_enter_time = svc_time_get_uptime_ms();
    s_need_refresh = true;
}

ui_page_t app_ui_get_current_page(void)
{
    return s_current_page;
}

void app_ui_handle_key(const key_event_t *event)
{
    if (!event) return;

    ESP_LOGI(TAG, "Key: id=%d, event=%d", event->key_id, event->event);

    // 长按确认键5秒：进入配网模式（仅在HOME页面）
    if (event->key_id == KEY_ID_K8 &&
        event->event == KEY_EVENT_LONG_PRESS &&
        event->duration_ms >= 5000 &&
        s_current_page == UI_PAGE_HOME) {
        ESP_LOGI(TAG, "Enter config mode");
        s_current_page = UI_PAGE_CONFIG;
        s_page_enter_time = svc_time_get_uptime_ms();
        s_need_refresh = true;
        return;
    }

    switch (s_current_page) {
    case UI_PAGE_HOME:
        if (event->key_id >= KEY_ID_K1 && event->key_id <= KEY_ID_K7) {
            // 类别键：进入录入状态
            s_selected_key = event->key_id;
            s_current_page = UI_PAGE_RECORD;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Selected category: %d", s_selected_key);
        } else if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            // 确认键短按：进入列表
            if (svc_storage_count() > 0) {
                s_current_page = UI_PAGE_LIST;
                s_list_current_index = 0;
                s_page_enter_time = svc_time_get_uptime_ms();
                s_last_list_scroll = svc_time_get_uptime_ms();
                s_need_refresh = true;
                ESP_LOGI(TAG, "Enter list mode");
            }
        }
        break;

    case UI_PAGE_RECORD:
        if (event->key_id >= KEY_ID_K1 && event->key_id <= KEY_ID_K7) {
            // 更换选中类别
            s_selected_key = event->key_id;
            s_need_refresh = true;
        } else if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            // 确认录入
            if (s_selected_key >= KEY_ID_K1 && s_selected_key <= KEY_ID_K7) {
                if (app_food_record(s_selected_key, &s_last_record) == 0) {
                    ESP_LOGI(TAG, "Record success");
                    s_need_refresh = true;
                    // 2秒后返回首页（在update中处理）
                }
            }
        }
        break;

    case UI_PAGE_LIST:
        if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            // 确认键：返回首页
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Return to home");
        }
        break;

    case UI_PAGE_ALERT:
        if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            // 确认键：关闭告警
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Dismiss alert");
        }
        break;

    case UI_PAGE_CONFIG:
        if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_LONG_PRESS && event->duration_ms >= 3000) {
            // 长按确认键3秒：退出配网
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Exit config mode");
        }
        break;
    }
}

void app_ui_handle_timeout(void)
{
    int64_t now = svc_time_get_uptime_ms();
    int64_t elapsed = now - s_page_enter_time;

    switch (s_current_page) {
    case UI_PAGE_RECORD:
        // 录入页2秒超时返回首页
        if (elapsed >= TIMEOUT_RECORD_MS) {
            ESP_LOGI(TAG, "Record timeout, return home");
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = now;
            s_need_refresh = true;
        }
        break;

    case UI_PAGE_LIST:
        // 列表页30秒超时返回首页
        if (elapsed >= TIMEOUT_LIST_MS) {
            ESP_LOGI(TAG, "List timeout, return home");
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = now;
            s_need_refresh = true;
        }
        break;

    case UI_PAGE_CONFIG:
        // 配网页120秒超时
        if (elapsed >= 120000) {
            ESP_LOGI(TAG, "Config timeout, return home");
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = now;
            s_need_refresh = true;
        }
        break;

    default:
        break;
    }
}

void app_ui_handle_expiry_check(bool has_expiring, bool has_expired)
{
    s_has_expiring = has_expiring;
    s_has_expired = has_expired;

    // 如果在首页且有过期/临期食材，显示告警页
    if (s_current_page == UI_PAGE_HOME && (has_expiring || has_expired)) {
        s_current_page = UI_PAGE_ALERT;
        s_page_enter_time = svc_time_get_uptime_ms();
        s_need_refresh = true;
        ESP_LOGI(TAG, "Show alert: expiring=%d, expired=%d", has_expiring, has_expired);
    }
}

void app_ui_update(void)
{
    int64_t now = svc_time_get_uptime_ms();

    // 处理超时
    app_ui_handle_timeout();

    // 列表页自动翻页
    if (s_current_page == UI_PAGE_LIST) {
        if (now - s_last_list_scroll >= LIST_AUTO_SCROLL_MS) {
            s_list_current_index++;
            if (s_list_current_index >= svc_storage_count()) {
                s_list_current_index = 0;
            }
            s_last_list_scroll = now;
            s_need_refresh = true;
        }
    }

    // 首页定期刷新时间显示（60秒）
    if (s_current_page == UI_PAGE_HOME) {
        if (now - s_last_refresh_time >= 60000) {
            s_need_refresh = true;
        }
    }

    // 刷新显示
    if (s_need_refresh) {
        app_ui_refresh();
        s_need_refresh = false;
        s_last_refresh_time = now;
    }
}

void app_ui_refresh(void)
{
    switch (s_current_page) {
    case UI_PAGE_HOME:
        draw_home_page();
        break;
    case UI_PAGE_RECORD:
        draw_record_page();
        break;
    case UI_PAGE_LIST:
        draw_list_page();
        break;
    case UI_PAGE_ALERT:
        draw_alert_page();
        break;
    case UI_PAGE_CONFIG:
        draw_config_page();
        break;
    }
}

// ============ 页面绘制函数 ============

static void draw_home_page(void)
{
    drv_st7789_fill_screen(COLOR_WHITE);

    // 状态栏：品牌 + 时间
    char time_buf[8];
    svc_time_format_time(svc_time_get_timestamp(), time_buf);
    drv_st7789_draw_string(10, 10, "Cloudy", COLOR_BLUE, COLOR_WHITE);
    drv_st7789_draw_string(180, 10, time_buf, COLOR_BLACK, COLOR_WHITE);

    // 主内容：统计信息
    food_stats_t stats;
    app_food_get_stats(&stats);

    char line1[32];
    char line2[32];

    if (stats.total_count == 0) {
        snprintf(line1, sizeof(line1), "No food items");
        snprintf(line2, sizeof(line2), "Press key to add");
    } else {
        snprintf(line1, sizeof(line1), "Total: %lu", (unsigned long)stats.total_count);

        if (stats.expired_count > 0) {
            snprintf(line2, sizeof(line2), "%lu EXPIRED!", (unsigned long)stats.expired_count);
        } else if (stats.expiring_count > 0) {
            snprintf(line2, sizeof(line2), "%lu expiring soon", (unsigned long)stats.expiring_count);
        } else {
            snprintf(line2, sizeof(line2), "All fresh");
        }
    }

    drv_st7789_draw_string(10, 80, line1, COLOR_BLACK, COLOR_WHITE);
    drv_st7789_draw_string(10, 110, line2, COLOR_BLACK, COLOR_WHITE);

    // 提示栏
    drv_st7789_draw_string(10, 240, "K1-K7: Add  K8: List", COLOR_GRAY, COLOR_WHITE);
}

static void draw_record_page(void)
{
    drv_st7789_fill_screen(COLOR_WHITE);

    // 获取类别配置
    const food_category_config_t *config = food_category_from_key(s_selected_key);
    if (!config) return;

    // 状态栏
    drv_st7789_draw_string(10, 10, "Recording", COLOR_BLUE, COLOR_WHITE);

    // 主内容：类别信息
    char line1[32];
    snprintf(line1, sizeof(line1), "%s %s", config->icon, config->name);
    drv_st7789_draw_string(10, 80, line1, COLOR_BLACK, COLOR_WHITE);

    // 保质期信息
    char line2[32];
    snprintf(line2, sizeof(line2), "Shelf: %d days", config->default_shelf_life);
    drv_st7789_draw_string(10, 110, line2, COLOR_BLACK, COLOR_WHITE);

    // 过期日期
    char line3[32];
    int64_t expiry = svc_time_get_timestamp() + (int64_t)config->default_shelf_life * 86400LL;
    char date_buf[6];
    svc_time_format_date(expiry, date_buf);
    snprintf(line3, sizeof(line3), "Expires: %s", date_buf);
    drv_st7789_draw_string(10, 140, line3, COLOR_BLACK, COLOR_WHITE);

    // 提示
    drv_st7789_draw_string(10, 240, "K8: Confirm", COLOR_GRAY, COLOR_WHITE);
}

static void draw_list_page(void)
{
    drv_st7789_fill_screen(COLOR_WHITE);

    uint32_t count = svc_storage_count();
    if (count == 0) {
        drv_st7789_draw_string_center(120, "No items", COLOR_BLACK, COLOR_WHITE, 2);
        return;
    }

    // 状态栏
    char header[32];
    snprintf(header, sizeof(header), "List %lu/%lu", (unsigned long)(s_list_current_index + 1), (unsigned long)count);
    drv_st7789_draw_string(10, 10, header, COLOR_BLUE, COLOR_WHITE);

    // 遍历找到第s_list_current_index条记录
    uint32_t found_index = 0;
    for (uint32_t id = 1; id <= count + 100; id++) {
        food_record_t record;
        if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
            if (found_index == s_list_current_index) {
                // 显示该记录
                const food_category_config_t *config = food_category_get_config(record.category);
                if (config) {
                    char line1[32];
                    snprintf(line1, sizeof(line1), "%s %s", config->icon, config->name);
                    drv_st7789_draw_string(10, 80, line1, COLOR_BLACK, COLOR_WHITE);
                }

                // 剩余天数
                int32_t days = app_food_get_remaining_days(&record);
                char line2[32];
                if (days < 0) {
                    snprintf(line2, sizeof(line2), "EXPIRED %ld days", (long)-days);
                    drv_st7789_draw_string(10, 110, line2, COLOR_RED, COLOR_WHITE);
                } else if (days == 0) {
                    drv_st7789_draw_string(10, 110, "Expires today!", COLOR_RED, COLOR_WHITE);
                } else if (days <= 2) {
                    snprintf(line2, sizeof(line2), "%ld days left", (long)days);
                    drv_st7789_draw_string(10, 110, line2, COLOR_YELLOW, COLOR_WHITE);
                } else {
                    snprintf(line2, sizeof(line2), "%ld days left", (long)days);
                    drv_st7789_draw_string(10, 110, line2, COLOR_GREEN, COLOR_WHITE);
                }
                break;
            }
            found_index++;
        }
    }

    // 提示
    drv_st7789_draw_string(10, 240, "K8: Back", COLOR_GRAY, COLOR_WHITE);
}

static void draw_alert_page(void)
{
    drv_st7789_fill_screen(COLOR_WHITE);

    // 状态栏
    if (s_has_expired) {
        drv_st7789_draw_string(10, 10, "EXPIRED!", COLOR_RED, COLOR_WHITE);
    } else if (s_has_expiring) {
        drv_st7789_draw_string(10, 10, "Expiring Soon", COLOR_YELLOW, COLOR_WHITE);
    }

    // 统计信息
    food_stats_t stats;
    app_food_get_stats(&stats);

    char line1[32];
    char line2[32];

    if (stats.expired_count > 0) {
        snprintf(line1, sizeof(line1), "%lu items EXPIRED", (unsigned long)stats.expired_count);
        drv_st7789_draw_string(10, 80, line1, COLOR_RED, COLOR_WHITE);
    }

    if (stats.expiring_count > 0) {
        snprintf(line2, sizeof(line2), "%lu items expiring", (unsigned long)stats.expiring_count);
        drv_st7789_draw_string(10, 110, line2, COLOR_YELLOW, COLOR_WHITE);
    }

    // 提示
    drv_st7789_draw_string(10, 240, "K8: Dismiss", COLOR_GRAY, COLOR_WHITE);
}

static void draw_config_page(void)
{
    drv_st7789_fill_screen(COLOR_WHITE);

    // 状态栏
    drv_st7789_draw_string(10, 10, "BLE Config", COLOR_BLUE, COLOR_WHITE);

    // 主内容
    drv_st7789_draw_string(10, 80, "Waiting for", COLOR_BLACK, COLOR_WHITE);
    drv_st7789_draw_string(10, 110, "connection...", COLOR_BLACK, COLOR_WHITE);

    // 设备ID
    drv_st7789_draw_string(10, 160, "ID: YX-001", COLOR_BLACK, COLOR_WHITE);

    // 提示
    drv_st7789_draw_string(10, 240, "Hold K8 3s to exit", COLOR_GRAY, COLOR_WHITE);
}
