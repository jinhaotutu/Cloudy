// App-Cloudy/product/app_ui.c
// UI 状态机模块实现 — 首页集成食材列表

#include "app_ui.h"
#include "app_food.h"
#include "drv_st7789.h"
#include "svc_time.h"
#include "wifi_mqtt.h"
#include "esp_log.h"
#include "cJSON.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "app_ui";

// 1.5x 字体函数（drv_st7789.c 中实现，头文件未声明）
extern void drv_st7789_fb_draw_string_1_5x(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

// 设备 ID（与 start.c 保持一致）
#ifndef DEVICE_ID
#define DEVICE_ID "device_001"
#endif

// 屏幕安全区域（圆角避让，四边距 16px）
#define SAFE_TOP        16
#define SAFE_BOTTOM     264
#define SAFE_LEFT       16
#define SAFE_RIGHT      224
#define SAFE_WIDTH      (SAFE_RIGHT - SAFE_LEFT)   // 208
#define SAFE_CENTER_X   (SAFE_LEFT + SAFE_WIDTH / 2)  // 120

// 字符宽度（8px/字符，scale=1）
#define CHAR_W          8

// 网格布局参数
#define GRID_COLS       2
#define GRID_ROWS       2
#define GRID_MAX        4
#define GRID_TOP        36          // 网格起始 Y
#define GRID_LEFT       SAFE_LEFT   // 网格起始 X
#define GRID_WIDTH      SAFE_WIDTH  // 208
#define GRID_HEIGHT     (SAFE_BOTTOM - GRID_TOP)  // 228
#define CELL_WIDTH      (GRID_WIDTH / GRID_COLS)   // 104
#define CELL_HEIGHT     (GRID_HEIGHT / GRID_ROWS)  // 114

// 辅助：计算居中 X 坐标
static uint16_t center_x(uint16_t str_len, uint8_t scale)
{
    uint16_t str_w = str_len * CHAR_W * scale;
    if (str_w >= SAFE_WIDTH) return SAFE_LEFT;
    return SAFE_LEFT + (SAFE_WIDTH - str_w) / 2;
}

// 超时配置（毫秒）
#define TIMEOUT_RECORD_MS       30000   // 录入确认页 30秒
#define TIMEOUT_ALERT_MS        0       // 告警页无自动超时
#define RECORD_COUNTDOWN_INTERVAL_MS 1000  // 倒计时刷新间隔 1秒
#define HOME_REFRESH_INTERVAL_MS 10000  // 首页刷新间隔 10秒

// 当前状态
static ui_page_t s_current_page = UI_PAGE_HOME;
static ui_page_t s_previous_page = UI_PAGE_HOME;
static int64_t s_page_enter_time = 0;
static int64_t s_last_refresh_time = 0;

// 录入状态
static uint8_t s_selected_key = 0;
static uint8_t s_last_selected_key = 0;
static food_record_t s_last_record;

// 首页列表滚动和选择
static uint32_t s_list_scroll_top = 0;   // 当前显示的第一条索引
static int32_t s_grid_selected = -1;     // 当前选中的格子 (0-3, -1=无选中)
static uint32_t s_sorted_ids[32];        // 排序后的 NVS 记录 ID
static int64_t s_select_time = 0;        // 上次选择操作时间
#define SELECT_TIMEOUT_MS   30000        // 选中无操作超时

// 刷新标志
static bool s_need_refresh = true;

// 前向声明 — 全屏绘制
static void draw_home_page(void);
static void draw_record_page(void);
static void draw_config_page(void);

// 前向声明 — 局部更新
static void update_record_countdown(void);

// ============ 类别图标绘制（48×48，帧缓冲） ============

static void draw_icon_dairy(int cx, int cy)
{
    uint16_t c1 = 0x4BF4;
    drv_st7789_fb_fill_rect(cx - 20, cy - 16, 40, 56, c1);
    drv_st7789_fb_fill_rect(cx - 12, cy - 32, 24, 20, c1);
    drv_st7789_fb_fill_rect(cx - 16, cy + 4, 32, 32, COLOR_WHITE);
    drv_st7789_fb_fill_rect(cx - 16, cy - 12, 32, 16, COLOR_WHITE);
}

static void draw_icon_meat(int cx, int cy)
{
    uint16_t c1 = 0xFFDF;
    uint16_t c2 = 0xFE60;
    static const int8_t egg_w[] = {0,8,12,16,18,20,22,22,24,24,24,24,24,24,24,22,22,20,18,16,12,8,0};
    for (int i = 0; i < 23; i++) {
        int y = cy - 36 + i * 3;
        if (egg_w[i] > 0)
            drv_st7789_fb_fill_rect(cx - egg_w[i], y, egg_w[i] * 2, 3, c1);
    }
    drv_st7789_fb_fill_rect(cx - 12, cy + 4, 24, 16, c2);
}

static void draw_icon_veggie(int cx, int cy)
{
    uint16_t c1 = 0xFD20;
    uint16_t c2 = 0x07E0;
    for (int y = 0; y < 10; y++) {
        int w = 8 + y * 4;
        drv_st7789_fb_fill_rect(cx - w, cy + y * 4, w * 2, 4, c1);
    }
    drv_st7789_fb_fill_rect(cx - 6, cy - 36, 12, 36, c2);
    drv_st7789_fb_fill_rect(cx - 20, cy - 28, 16, 8, c2);
    drv_st7789_fb_fill_rect(cx + 4, cy - 28, 16, 8, c2);
}

static void draw_icon_fruit(int cx, int cy)
{
    uint16_t c1 = 0xF800;
    uint16_t c2 = 0x07E0;
    uint16_t c3 = 0x7BEF;
    static const int8_t apple_w[] = {0,12,18,22,24,26,28,28,28,28,28,28,26,24,22,18,12,0};
    for (int i = 0; i < 18; i++) {
        int y = cy - 28 + i * 3;
        if (apple_w[i] > 0)
            drv_st7789_fb_fill_rect(cx - apple_w[i], y, apple_w[i] * 2, 3, c1);
    }
    drv_st7789_fb_fill_rect(cx - 6, cy - 32, 12, 8, c1);
    drv_st7789_fb_fill_rect(cx, cy - 40, 4, 12, c3);
    drv_st7789_fb_fill_rect(cx + 4, cy - 36, 16, 8, c2);
}

static void draw_icon_seafood(int cx, int cy)
{
    uint16_t c1 = 0x43FC;
    uint16_t c2 = 0xFFFF;
    uint16_t c3 = 0x0000;
    static const int8_t fish_w[] = {0,14,20,24,28,30,32,32,32,32,32,32,30,28,24,20,14,0};
    for (int i = 0; i < 18; i++) {
        int y = cy - 18 + i * 2;
        if (fish_w[i] > 0)
            drv_st7789_fb_fill_rect(cx - fish_w[i], y, fish_w[i] * 2, 2, c1);
    }
    for (int y = -12; y <= 12; y++) {
        int w = (y + 12) * 16 / 24;
        drv_st7789_fb_fill_rect(cx + 32, cy + y, w, 1, c1);
    }
    drv_st7789_fb_fill_rect(cx - 16, cy - 6, 10, 10, c2);
    drv_st7789_fb_draw_pixel(cx - 12, cy - 2, c3);
}

static void draw_icon_drink(int cx, int cy)
{
    uint16_t c1 = 0x07E0;
    uint16_t c2 = 0xC618;
    uint16_t c3 = 0xF800;
    for (int y = 0; y < 13; y++) {
        int w = 20 + y * 2;
        drv_st7789_fb_fill_rect(cx - w, cy - 16 + y * 4, w * 2, 4, c2);
    }
    drv_st7789_fb_fill_rect(cx - 24, cy + 4, 48, 28, c1);
    drv_st7789_fb_fill_rect(cx + 8, cy - 40, 6, 48, c3);
}

static void draw_icon_frozen(int cx, int cy)
{
    uint16_t c1 = 0x4BF4;
    int len = 36;
    drv_st7789_fb_fill_rect(cx - 2, cy - len, 5, len * 2, c1);
    drv_st7789_fb_fill_rect(cx - len, cy - 2, len * 2, 5, c1);
    for (int i = -len; i <= len; i += 2) {
        drv_st7789_fb_fill_rect(cx + i, cy + i, 3, 3, c1);
        drv_st7789_fb_fill_rect(cx + i, cy - i, 3, 3, c1);
    }
    drv_st7789_fb_fill_rect(cx - 8, cy - 8, 17, 17, c1);
}

typedef void (*icon_draw_fn)(int cx, int cy);

static const icon_draw_fn s_icon_drawers[] = {
    draw_icon_dairy,
    draw_icon_meat,
    draw_icon_veggie,
    draw_icon_fruit,
    draw_icon_seafood,
    draw_icon_drink,
    draw_icon_frozen,
};

static void draw_category_icon(uint8_t category, int cx, int cy)
{
    if (category >= 1 && category <= 7) {
        s_icon_drawers[category - 1](cx, cy);
    }
}

static void update_record_category(void);

void app_ui_init(void)
{
    ESP_LOGI(TAG, "UI init");

    drv_st7789_fb_init();
    drv_st7789_fill_screen(COLOR_WHITE);

    drv_st7789_draw_string_center(60, "Cloudy", COLOR_BLUE, COLOR_WHITE, 3);
    drv_st7789_draw_string_center(120, "Starting...", COLOR_BLACK, COLOR_WHITE, 2);

    s_current_page = UI_PAGE_HOME;
    s_previous_page = UI_PAGE_RECORD;  // 确保首次刷新触发全屏绘制
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
        if (event->key_id >= KEY_ID_K1 && event->key_id <= KEY_ID_K7 &&
            event->event == KEY_EVENT_SHORT_PRESS) {
            // 类别键短按：进入录入状态
            s_selected_key = event->key_id;
            s_current_page = UI_PAGE_RECORD;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Selected category: %d", s_selected_key);
        } else if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            // K8短按：循环选择食材（跨页）
            uint32_t total = svc_storage_count();
            if (total > 0) {
                // 当前页内选中索引 + 全局偏移
                uint32_t global_idx = s_list_scroll_top + s_grid_selected + 1;
                if (s_grid_selected < 0) global_idx = s_list_scroll_top;

                if (global_idx >= total) {
                    // 到末尾，回到第一页第一个
                    s_list_scroll_top = 0;
                    s_grid_selected = 0;
                } else {
                    // 检查是否需要翻页
                    uint32_t page_offset = global_idx % GRID_MAX;
                    s_list_scroll_top = (global_idx / GRID_MAX) * GRID_MAX;
                    s_grid_selected = (int32_t)page_offset;
                }
                s_need_refresh = true;
                s_select_time = svc_time_get_uptime_ms();
                ESP_LOGI(TAG, "Grid select: global=%lu, scroll=%lu, sel=%ld",
                         (unsigned long)(s_list_scroll_top + s_grid_selected),
                         (unsigned long)s_list_scroll_top, (long)s_grid_selected);
            }
        } else if (event->key_id == KEY_ID_K9 && event->event == KEY_EVENT_SHORT_PRESS) {
            // K9短按：删除选中的食材（通过 NVS ID）
            if (s_grid_selected >= 0) {
                uint32_t global_idx = s_list_scroll_top + s_grid_selected;
                uint32_t nvs_id = s_sorted_ids[global_idx];

                // 删除前获取食材信息
                food_record_t del_record;
                const food_category_config_t *del_cfg = NULL;
                if (svc_storage_get(nvs_id, &del_record) == SVC_STORAGE_OK) {
                    del_cfg = food_category_get_config(del_record.category);
                }

                if (app_food_delete_by_nvs_id(nvs_id) == 0) {
                    ESP_LOGI(TAG, "Deleted item global_idx=%lu nvs_id=%lu",
                             (unsigned long)global_idx, (unsigned long)nvs_id);

                    // 发布 delete 事件到 MQTT
                    cJSON *evt = cJSON_CreateObject();
                    cJSON_AddStringToObject(evt, "action", "delete");
                    cJSON_AddNumberToObject(evt, "category", del_record.category);
                    if (del_cfg) {
                        cJSON_AddStringToObject(evt, "category_name", del_cfg->name);
                    }
                    char *json_str = cJSON_PrintUnformatted(evt);
                    if (json_str) {
                        mqtt_publish_event(DEVICE_ID, json_str);
                        free(json_str);
                    }
                    cJSON_Delete(evt);

                    // 调整选中位置
                    uint32_t remaining = svc_storage_count();
                    if (remaining == 0) {
                        s_grid_selected = -1;
                        s_list_scroll_top = 0;
                    } else if (global_idx >= remaining) {
                        s_list_scroll_top = (remaining > GRID_MAX) ?
                                             ((remaining - 1) / GRID_MAX) * GRID_MAX : 0;
                        s_grid_selected = (int32_t)((remaining - 1) % GRID_MAX);
                    }
                    s_need_refresh = true;
                }
            }
        }
        break;

    case UI_PAGE_RECORD:
        if (event->key_id == KEY_ID_K9 && event->event == KEY_EVENT_SHORT_PRESS) {
            // K9短按：退出录入，返回首页
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Cancel record, return home");
        } else if (event->key_id >= KEY_ID_K1 && event->key_id <= KEY_ID_K7 &&
            event->event == KEY_EVENT_SHORT_PRESS) {
            s_selected_key = event->key_id;
            s_need_refresh = true;
        } else if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            if (s_selected_key >= KEY_ID_K1 && s_selected_key <= KEY_ID_K7) {
                if (app_food_record(s_selected_key, &s_last_record) == 0) {
                    ESP_LOGI(TAG, "Record success");

                    // 发布 add 事件到 MQTT
                    const food_category_config_t *cfg = food_category_get_config(s_last_record.category);
                    cJSON *evt = cJSON_CreateObject();
                    cJSON_AddStringToObject(evt, "action", "add");
                    cJSON_AddNumberToObject(evt, "category", s_last_record.category);
                    cJSON_AddNumberToObject(evt, "shelf_life", s_last_record.shelf_life);
                    if (cfg) {
                        cJSON_AddStringToObject(evt, "category_name", cfg->name);
                    }
                    char *json_str = cJSON_PrintUnformatted(evt);
                    if (json_str) {
                        mqtt_publish_event(DEVICE_ID, json_str);
                        ESP_LOGI(TAG, "Published add event: %s", json_str);
                        free(json_str);
                    }
                    cJSON_Delete(evt);

                    s_current_page = UI_PAGE_HOME;
                    s_page_enter_time = svc_time_get_uptime_ms();
                    s_need_refresh = true;
                }
            }
        }
        break;

    case UI_PAGE_CONFIG:
        if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_LONG_PRESS && event->duration_ms >= 3000) {
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = svc_time_get_uptime_ms();
            s_need_refresh = true;
            ESP_LOGI(TAG, "Exit config mode");
        }
        break;
    }
}

static void handle_timeout(void)
{
    int64_t now = svc_time_get_uptime_ms();
    int64_t elapsed = now - s_page_enter_time;

    switch (s_current_page) {
    case UI_PAGE_RECORD:
        if (elapsed >= TIMEOUT_RECORD_MS) {
            ESP_LOGI(TAG, "Record timeout, return home");
            s_current_page = UI_PAGE_HOME;
            s_page_enter_time = now;
            s_need_refresh = true;
        }
        break;

    case UI_PAGE_CONFIG:
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

void app_ui_update(void)
{
    int64_t now = svc_time_get_uptime_ms();

    handle_timeout();

    // 首页定期刷新（选中时 1 秒刷新倒计时，否则 10 秒刷新时间）
    if (s_current_page == UI_PAGE_HOME) {
        int64_t refresh_interval = (s_grid_selected >= 0) ? 1000 : HOME_REFRESH_INTERVAL_MS;
        if (now - s_last_refresh_time >= refresh_interval) {
            s_need_refresh = true;
        }
        // 选中无操作 30 秒，自动取消选中并回到第一页
        if (s_grid_selected >= 0 && s_select_time > 0 &&
            (now - s_select_time) >= SELECT_TIMEOUT_MS) {
            s_grid_selected = -1;
            s_list_scroll_top = 0;
            s_need_refresh = true;
            ESP_LOGI(TAG, "Selection timeout, reset to page 1");
        }
    }

    // 录入页倒计时每秒刷新
    if (s_current_page == UI_PAGE_RECORD) {
        if (now - s_last_refresh_time >= RECORD_COUNTDOWN_INTERVAL_MS) {
            s_need_refresh = true;
        }
    }

    if (s_need_refresh) {
        app_ui_refresh();
        s_need_refresh = false;
        s_last_refresh_time = now;
    }
}

void app_ui_refresh(void)
{
    bool page_changed = (s_current_page != s_previous_page);

    if (page_changed) {
        switch (s_current_page) {
        case UI_PAGE_HOME:
            draw_home_page();
            break;
        case UI_PAGE_RECORD:
            draw_record_page();
            break;
        case UI_PAGE_CONFIG:
            draw_config_page();
            break;
        }
        s_previous_page = s_current_page;
    } else {
        switch (s_current_page) {
        case UI_PAGE_HOME:
            draw_home_page();  // 列表内容变化，全屏刷新
            break;
        case UI_PAGE_RECORD:
            if (s_selected_key != s_last_selected_key) {
                update_record_category();
                s_last_selected_key = s_selected_key;
            } else {
                update_record_countdown();
            }
            break;
        default:
            break;
        }
    }
}

// ============ 首页绘制（2×2 网格） ============

// 图标绘制参数
#define ICON_DRAW_SIZE  32
#define ICON_TOP_PAD    6           // 图标顶部留白

// 虚线颜色（浅灰）
#define DASH_COLOR      0xC618      // 浅灰色

// 绘制虚线（水平）
static void draw_dash_h(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    for (uint16_t i = 0; i < len; i += 8) {
        uint16_t seg = (len - i > 4) ? 4 : (len - i);
        drv_st7789_fb_fill_rect(x + i, y, seg, 1, color);
    }
}

// 绘制虚线（垂直）
static void draw_dash_v(uint16_t x, uint16_t y, uint16_t len, uint16_t color)
{
    for (uint16_t i = 0; i < len; i += 8) {
        uint16_t seg = (len - i > 4) ? 4 : (len - i);
        drv_st7789_fb_fill_rect(x, y + i, 1, seg, color);
    }
}

// ============ 小图标绘制（32×32，用于网格首页） ============

static void draw_icon_small_dairy(int cx, int cy)
{
    uint16_t c1 = 0x4BF4;
    drv_st7789_fb_fill_rect(cx - 10, cy - 8, 20, 28, c1);
    drv_st7789_fb_fill_rect(cx - 6, cy - 16, 12, 10, c1);
    drv_st7789_fb_fill_rect(cx - 8, cy + 2, 16, 16, COLOR_WHITE);
    drv_st7789_fb_fill_rect(cx - 8, cy - 6, 16, 8, COLOR_WHITE);
}

static void draw_icon_small_meat(int cx, int cy)
{
    uint16_t c1 = 0xFFDF;
    uint16_t c2 = 0xFE60;
    static const int8_t w[] = {0,6,10,12,14,14,14,14,12,10,6,0};
    for (int i = 0; i < 12; i++) {
        int y = cy - 18 + i * 3;
        if (w[i] > 0)
            drv_st7789_fb_fill_rect(cx - w[i], y, w[i] * 2, 3, c1);
    }
    drv_st7789_fb_fill_rect(cx - 6, cy + 2, 12, 8, c2);
}

static void draw_icon_small_veggie(int cx, int cy)
{
    uint16_t c1 = 0xFD20;
    uint16_t c2 = 0x07E0;
    for (int y = 0; y < 6; y++) {
        int w = 4 + y * 3;
        drv_st7789_fb_fill_rect(cx - w, cy + y * 3, w * 2, 3, c1);
    }
    drv_st7789_fb_fill_rect(cx - 3, cy - 18, 6, 18, c2);
    drv_st7789_fb_fill_rect(cx - 10, cy - 14, 8, 4, c2);
    drv_st7789_fb_fill_rect(cx + 2, cy - 14, 8, 4, c2);
}

static void draw_icon_small_fruit(int cx, int cy)
{
    uint16_t c1 = 0xF800;
    uint16_t c2 = 0x07E0;
    uint16_t c3 = 0x7BEF;
    static const int8_t w[] = {0,8,12,14,16,16,16,14,12,8,0};
    for (int i = 0; i < 11; i++) {
        int y = cy - 14 + i * 3;
        if (w[i] > 0)
            drv_st7789_fb_fill_rect(cx - w[i], y, w[i] * 2, 3, c1);
    }
    drv_st7789_fb_fill_rect(cx - 3, cy - 16, 6, 4, c1);
    drv_st7789_fb_fill_rect(cx, cy - 20, 2, 6, c3);
    drv_st7789_fb_fill_rect(cx + 2, cy - 18, 8, 4, c2);
}

static void draw_icon_small_seafood(int cx, int cy)
{
    uint16_t c1 = 0x43FC;
    uint16_t c2 = 0xFFFF;
    static const int8_t w[] = {0,8,12,16,18,18,18,16,12,8,0};
    for (int i = 0; i < 11; i++) {
        int y = cy - 10 + i * 2;
        if (w[i] > 0)
            drv_st7789_fb_fill_rect(cx - w[i], y, w[i] * 2, 2, c1);
    }
    for (int y = -6; y <= 6; y++) {
        int w2 = (y + 6) * 8 / 12;
        drv_st7789_fb_fill_rect(cx + 18, cy + y, w2, 1, c1);
    }
    drv_st7789_fb_fill_rect(cx - 8, cy - 4, 6, 6, c2);
    drv_st7789_fb_draw_pixel(cx - 5, cy - 1, 0x0000);
}

static void draw_icon_small_drink(int cx, int cy)
{
    uint16_t c1 = 0x07E0;
    uint16_t c2 = 0xC618;
    uint16_t c3 = 0xF800;
    for (int y = 0; y < 8; y++) {
        int w = 12 + y * 2;
        drv_st7789_fb_fill_rect(cx - w, cy - 10 + y * 3, w * 2, 3, c2);
    }
    drv_st7789_fb_fill_rect(cx - 14, cy + 4, 28, 14, c1);
    drv_st7789_fb_fill_rect(cx + 4, cy - 20, 4, 24, c3);
}

static void draw_icon_small_frozen(int cx, int cy)
{
    uint16_t c1 = 0x4BF4;
    int len = 18;
    drv_st7789_fb_fill_rect(cx - 1, cy - len, 3, len * 2, c1);
    drv_st7789_fb_fill_rect(cx - len, cy - 1, len * 2, 3, c1);
    for (int i = -len; i <= len; i += 3) {
        drv_st7789_fb_fill_rect(cx + i, cy + i, 2, 2, c1);
        drv_st7789_fb_fill_rect(cx + i, cy - i, 2, 2, c1);
    }
    drv_st7789_fb_fill_rect(cx - 4, cy - 4, 9, 9, c1);
}

typedef void (*icon_draw_fn)(int cx, int cy);

static const icon_draw_fn s_icon_small_drawers[] = {
    draw_icon_small_dairy,
    draw_icon_small_meat,
    draw_icon_small_veggie,
    draw_icon_small_fruit,
    draw_icon_small_seafood,
    draw_icon_small_drink,
    draw_icon_small_frozen,
};

static void draw_category_icon_small(uint8_t category, int cx, int cy)
{
    if (category >= 1 && category <= 7) {
        s_icon_small_drawers[category - 1](cx, cy);
    }
}

// 1.5x 字体宽度
#define FONT15_W        12

static void draw_home_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    // 状态栏：品牌居左 + 时间/倒计时居右
    drv_st7789_fb_draw_string(SAFE_LEFT, SAFE_TOP, "Cloudy", COLOR_BLUE, COLOR_WHITE);

    if (s_grid_selected >= 0 && s_select_time > 0) {
        // 选中状态：显示倒计时
        int64_t now = svc_time_get_uptime_ms();
        int64_t remaining_ms = SELECT_TIMEOUT_MS - (now - s_select_time);
        if (remaining_ms < 0) remaining_ms = 0;
        int32_t remaining_sec = (int32_t)(remaining_ms / 1000);
        char countdown[8];
        snprintf(countdown, sizeof(countdown), "%2lds", (long)remaining_sec);
        uint16_t cd_w = strlen(countdown) * CHAR_W;
        drv_st7789_fb_draw_string(SAFE_RIGHT - cd_w, SAFE_TOP, countdown, COLOR_RED, COLOR_WHITE);
    } else {
        // 正常状态：显示时间
        int64_t ts = svc_time_get_timestamp();
        char time_buf[8];
        svc_time_format_time(ts, time_buf);
        uint16_t time_w = strlen(time_buf) * CHAR_W;
        drv_st7789_fb_draw_string(SAFE_RIGHT - time_w, SAFE_TOP, time_buf, COLOR_BLACK, COLOR_WHITE);
    }

    // 获取排序后的食材列表（同时记录 NVS ID）
    food_record_t items[32];
    uint32_t count = app_food_get_sorted(items, s_sorted_ids, 32);

    if (count == 0) {
        drv_st7789_fb_draw_string(center_x(13, 1), 110, "No food items", COLOR_GRAY, COLOR_WHITE);
        drv_st7789_fb_draw_string(center_x(15, 1), 140, "Press key to add", COLOR_GRAY, COLOR_WHITE);
        drv_st7789_fb_flush();
        return;
    }

    // 虚线分隔线
    uint16_t mid_x = GRID_LEFT + CELL_WIDTH;
    uint16_t mid_y = GRID_TOP + CELL_HEIGHT;
    draw_dash_v(mid_x, GRID_TOP, GRID_HEIGHT, DASH_COLOR);
    draw_dash_h(GRID_LEFT, mid_y, GRID_WIDTH, DASH_COLOR);

    // 绘制 2×2 网格
    uint32_t show_count = (count > GRID_MAX) ? GRID_MAX : count;

    for (uint32_t i = 0; i < show_count; i++) {
        uint32_t idx = s_list_scroll_top + i;
        if (idx >= count) break;

        food_record_t *rec = &items[idx];
        const food_category_config_t *cfg = food_category_get_config(rec->category);

        // 计算网格位置
        uint16_t col = i % GRID_COLS;
        uint16_t row = i / GRID_COLS;
        uint16_t cell_x = GRID_LEFT + col * CELL_WIDTH;
        uint16_t cell_y = GRID_TOP + row * CELL_HEIGHT;

        // 内容垂直居中：icon(32) + gap(4) + name(8) + gap(8) + days(8) = 60
        #define CONTENT_H  (ICON_DRAW_SIZE + 4 + 8 + 8 + 8)
        uint16_t content_top = cell_y + (CELL_HEIGHT - CONTENT_H) / 2;

        // 图标（水平居中）
        uint16_t icon_cx = cell_x + CELL_WIDTH / 2;
        uint16_t icon_cy = content_top + ICON_DRAW_SIZE / 2;
        draw_category_icon_small(rec->category, icon_cx, icon_cy);

        // 类别名（居中）
        const char *name = cfg ? cfg->name : "???";
        uint16_t name_w = strlen(name) * CHAR_W;
        uint16_t name_x = cell_x + (CELL_WIDTH - name_w) / 2;
        uint16_t name_y = content_top + ICON_DRAW_SIZE + 4;
        drv_st7789_fb_draw_string(name_x, name_y, name, COLOR_BLACK, COLOR_WHITE);

        // 剩余天数（居中，颜色区分状态）
        char days_str[20];
        uint16_t color = COLOR_BLACK;
        int32_t remaining_sec = app_food_get_remaining_sec(rec);
        int32_t days = remaining_sec / 86400;

        if (days < 0) {
            snprintf(days_str, sizeof(days_str), "%ldd!", (long)(-days));
            color = COLOR_RED;
        } else if (days == 0) {
            snprintf(days_str, sizeof(days_str), "today!");
            color = COLOR_YELLOW;
        } else if (remaining_sec <= 2 * 86400) {
            snprintf(days_str, sizeof(days_str), "%ldd left", (long)days);
            color = COLOR_YELLOW;
        } else {
            snprintf(days_str, sizeof(days_str), "%ldd left", (long)days);
            color = COLOR_BLACK;
        }
        uint16_t days_w = strlen(days_str) * CHAR_W;
        uint16_t days_x = cell_x + (CELL_WIDTH - days_w) / 2;
        uint16_t days_y = name_y + 16;
        drv_st7789_fb_draw_string(days_x, days_y, days_str, color, COLOR_WHITE);

        // 选中箭头指示器
        if ((int32_t)i == s_grid_selected) {
            uint16_t arrow_x = cell_x + 2;
            uint16_t arrow_y = cell_y + CELL_HEIGHT / 2 - 4;
            // 绘制实心三角形 ">"
            for (int a = 0; a < 8; a++) {
                drv_st7789_fb_fill_rect(arrow_x + a, arrow_y + a, 2, 1, COLOR_BLUE);
                drv_st7789_fb_fill_rect(arrow_x + a, arrow_y + 15 - a, 2, 1, COLOR_BLUE);
            }
            drv_st7789_fb_fill_rect(arrow_x + 7, arrow_y + 4, 2, 8, COLOR_BLUE);
        }
    }

    // 底部：翻页提示
    if (count > GRID_MAX) {
        uint32_t page = s_list_scroll_top / GRID_MAX + 1;
        uint32_t total_pages = (count + GRID_MAX - 1) / GRID_MAX;
        char hint[32];
        snprintf(hint, sizeof(hint), "K8: Page %lu/%lu", (unsigned long)page, (unsigned long)total_pages);
        drv_st7789_fb_draw_string(center_x(strlen(hint), 1), SAFE_BOTTOM - 16, hint, COLOR_GRAY, COLOR_WHITE);
    } else {
        drv_st7789_fb_draw_string(center_x(10, 1), SAFE_BOTTOM - 16, "K1-K7: Add", COLOR_GRAY, COLOR_WHITE);
    }

    drv_st7789_fb_flush();
}

// ============ 录入页 ============

static void draw_record_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    const food_category_config_t *config = food_category_from_key(s_selected_key);
    if (!config) return;

    drv_st7789_fb_draw_string(SAFE_LEFT, SAFE_TOP, "Cloudy", COLOR_BLUE, COLOR_WHITE);

    // 倒计时（右上角）
    int64_t now = svc_time_get_uptime_ms();
    int64_t remaining_ms = TIMEOUT_RECORD_MS - (now - s_page_enter_time);
    if (remaining_ms < 0) remaining_ms = 0;
    int32_t remaining_sec = (int32_t)(remaining_ms / 1000);
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%3lds", (long)remaining_sec);
    drv_st7789_fb_draw_string(SAFE_RIGHT - 32, SAFE_TOP, countdown, COLOR_RED, COLOR_WHITE);

    draw_category_icon(config->category, SAFE_CENTER_X, 108);

    char info[32];
    snprintf(info, sizeof(info), "%s: %d days", config->name, config->default_shelf_life);
    drv_st7789_fb_draw_string_center_1_5x(172, info, COLOR_BLACK, COLOR_WHITE);

    drv_st7789_fb_draw_string(center_x(11, 1), SAFE_BOTTOM - 16, "K8: Confirm", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();
    s_last_selected_key = s_selected_key;
}

// ============ 局部更新函数 ============

static void update_record_countdown(void)
{
    int64_t now = svc_time_get_uptime_ms();
    int64_t remaining_ms = TIMEOUT_RECORD_MS - (now - s_page_enter_time);
    if (remaining_ms < 0) remaining_ms = 0;
    int32_t remaining_sec = (int32_t)(remaining_ms / 1000);
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%3lds", (long)remaining_sec);
    drv_st7789_draw_string(SAFE_RIGHT - 32, SAFE_TOP, countdown, COLOR_RED, COLOR_WHITE);
}

static void update_record_category(void)
{
    draw_record_page();
}

// ============ 配网页 ============

// ============ 配网页 ============

static void draw_config_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    drv_st7789_fb_draw_string(center_x(10, 1), SAFE_TOP, "BLE Config", COLOR_BLUE, COLOR_WHITE);
    drv_st7789_fb_draw_string(center_x(11, 1), 90, "Waiting for", COLOR_BLACK, COLOR_WHITE);
    drv_st7789_fb_draw_string(center_x(12, 1), 120, "connection...", COLOR_BLACK, COLOR_WHITE);
    drv_st7789_fb_draw_string(center_x(10, 1), 160, "ID: YX-001", COLOR_BLACK, COLOR_WHITE);
    drv_st7789_fb_draw_string(center_x(18, 1), SAFE_BOTTOM - 16, "Hold K8 3s to exit", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();
}
