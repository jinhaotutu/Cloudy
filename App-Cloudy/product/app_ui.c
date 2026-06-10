// App-Cloudy/product/app_ui.c
// UI 状态机模块实现

#include "app_ui.h"
#include "app_food.h"
#include "drv_st7789.h"
#include "svc_time.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "app_ui";

// 屏幕安全区域（圆角避让，四边距 16px）
#define SAFE_TOP        16
#define SAFE_BOTTOM     264
#define SAFE_LEFT       16
#define SAFE_RIGHT      224
#define SAFE_WIDTH      (SAFE_RIGHT - SAFE_LEFT)   // 208
#define SAFE_CENTER_X   (SAFE_LEFT + SAFE_WIDTH / 2)  // 120

// 字符宽度（8px/字符，scale=1）
#define CHAR_W          8

// 辅助：计算居中 X 坐标
static uint16_t center_x(uint16_t str_len, uint8_t scale)
{
    uint16_t str_w = str_len * CHAR_W * scale;
    if (str_w >= SAFE_WIDTH) return SAFE_LEFT;
    return SAFE_LEFT + (SAFE_WIDTH - str_w) / 2;
}

// 超时配置（毫秒）
#define TIMEOUT_RECORD_MS       30000   // 录入确认页 30秒
#define TIMEOUT_LIST_MS         30000   // 列表页 30秒
#define TIMEOUT_ALERT_MS        0       // 告警页无自动超时
#define LIST_AUTO_SCROLL_MS     3000    // 列表自动翻页 3秒
#define RECORD_COUNTDOWN_INTERVAL_MS 1000  // 倒计时刷新间隔 1秒

// 当前状态
static ui_page_t s_current_page = UI_PAGE_HOME;
static ui_page_t s_previous_page = UI_PAGE_HOME;  // 上一次页面（用于检测页面切换）
static int64_t s_page_enter_time = 0;       // 进入当前页面的时间
static int64_t s_last_refresh_time = 0;     // 上次刷新时间
static int64_t s_last_list_scroll = 0;      // 列表页上次翻页时间

// 录入状态
static uint8_t s_selected_key = 0;          // 当前选中的类别键
static uint8_t s_last_selected_key = 0;     // 上次选中的类别（检测类别切换）
static food_record_t s_last_record;         // 最后录入的记录

// 列表状态
static uint32_t s_list_current_index = 0;   // 列表当前显示索引
static uint32_t s_last_list_index = 0;      // 上次显示的索引（检测翻页）

// 告警状态
static bool s_has_expiring = false;
static bool s_has_expired = false;

// 刷新标志
static bool s_need_refresh = true;

// 前向声明 — 全屏绘制
static void draw_home_page(void);
static void draw_record_page(void);
static void draw_list_page(void);
static void draw_alert_page(void);
static void draw_config_page(void);

// 前向声明 — 局部更新
static void update_home_time(void);
static void update_record_countdown(void);

// ============ 类别图标绘制（48×48，帧缓冲） ============

#define ICON_SIZE   48
#define ICON_CX     (SAFE_CENTER_X)
#define ICON_CY     60

static void draw_icon_dairy(int cx, int cy)
{
    // 牛奶瓶：浅蓝瓶身 + 白色牛奶（2x）
    uint16_t c1 = 0x4BF4;
    drv_st7789_fb_fill_rect(cx - 20, cy - 16, 40, 56, c1);
    drv_st7789_fb_fill_rect(cx - 12, cy - 32, 24, 20, c1);
    drv_st7789_fb_fill_rect(cx - 16, cy + 4, 32, 32, COLOR_WHITE);
    drv_st7789_fb_fill_rect(cx - 16, cy - 12, 32, 16, COLOR_WHITE);
}

static void draw_icon_meat(int cx, int cy)
{
    // 鸡蛋：用矩形逐行近似椭圆（2x）
    uint16_t c1 = 0xFFDF;
    uint16_t c2 = 0xFE60;
    static const int8_t egg_w[] = {0,8,12,16,18,20,22,22,24,24,24,24,24,24,24,22,22,20,18,16,12,8,0};
    for (int i = 0; i < 23; i++) {
        int y = cy - 36 + i * 3;
        if (egg_w[i] > 0)
            drv_st7789_fb_fill_rect(cx - egg_w[i], y, egg_w[i] * 2, 3, c1);
    }
    // 蛋黄
    drv_st7789_fb_fill_rect(cx - 12, cy + 4, 24, 16, c2);
}

static void draw_icon_veggie(int cx, int cy)
{
    // 胡萝卜：矩形近似（2x）
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
    // 苹果：用矩形逐行近似圆形（2x）
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
    // 鱼：矩形近似椭圆 + 三角尾巴（2x）
    uint16_t c1 = 0x43FC;
    uint16_t c2 = 0xFFFF;
    uint16_t c3 = 0x0000;
    // 鱼身（椭圆近似）
    static const int8_t fish_w[] = {0,14,20,24,28,30,32,32,32,32,32,32,30,28,24,20,14,0};
    for (int i = 0; i < 18; i++) {
        int y = cy - 18 + i * 2;
        if (fish_w[i] > 0)
            drv_st7789_fb_fill_rect(cx - fish_w[i], y, fish_w[i] * 2, 2, c1);
    }
    // 尾巴
    for (int y = -12; y <= 12; y++) {
        int w = (y + 12) * 16 / 24;
        drv_st7789_fb_fill_rect(cx + 32, cy + y, w, 1, c1);
    }
    // 眼睛
    drv_st7789_fb_fill_rect(cx - 16, cy - 6, 10, 10, c2);
    drv_st7789_fb_draw_pixel(cx - 12, cy - 2, c3);
}

static void draw_icon_drink(int cx, int cy)
{
    // 杯子：梯形用矩形逐行近似（2x）
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
    // 雪花：矩形（2x）
    uint16_t c1 = 0x4BF4;
    int len = 36;
    drv_st7789_fb_fill_rect(cx - 2, cy - len, 5, len * 2, c1);
    drv_st7789_fb_fill_rect(cx - len, cy - 2, len * 2, 5, c1);
    // 对角线用短线段近似
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
static void update_list_countdown(void);
static void update_list_item(void);
static void draw_list_header(void);
static void draw_list_item_content(uint32_t index);

void app_ui_init(void)
{
    ESP_LOGI(TAG, "UI init");

    // 初始化帧缓冲
    drv_st7789_fb_init();

    // 清屏
    drv_st7789_fill_screen(COLOR_WHITE);

    // 显示启动信息
    drv_st7789_draw_string_center(60, "Cloudy", COLOR_BLUE, COLOR_WHITE, 3);
    drv_st7789_draw_string_center(120, "Starting...", COLOR_BLACK, COLOR_WHITE, 2);

    s_current_page = UI_PAGE_HOME;
    // 设为不同值，确保首次刷新触发全屏绘制
    s_previous_page = UI_PAGE_RECORD;
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
        if (event->key_id >= KEY_ID_K1 && event->key_id <= KEY_ID_K7 &&
            event->event == KEY_EVENT_SHORT_PRESS) {
            // 短按类别键：更换选中类别
            s_selected_key = event->key_id;
            s_need_refresh = true;
        } else if (event->key_id == KEY_ID_K8 && event->event == KEY_EVENT_SHORT_PRESS) {
            // 确认录入，立即返回首页
            if (s_selected_key >= KEY_ID_K1 && s_selected_key <= KEY_ID_K7) {
                if (app_food_record(s_selected_key, &s_last_record) == 0) {
                    ESP_LOGI(TAG, "Record success");
                    s_current_page = UI_PAGE_HOME;
                    s_page_enter_time = svc_time_get_uptime_ms();
                    s_need_refresh = true;
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
        // 录入页30秒超时返回首页
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
        // 倒计时每秒刷新
        if (now - s_last_refresh_time >= RECORD_COUNTDOWN_INTERVAL_MS) {
            s_need_refresh = true;
        }
    }

    // 首页定期刷新时间显示（60秒）
    if (s_current_page == UI_PAGE_HOME) {
        if (now - s_last_refresh_time >= 60000) {
            s_need_refresh = true;
        }
    }

    // 录入页倒计时每秒刷新
    if (s_current_page == UI_PAGE_RECORD) {
        if (now - s_last_refresh_time >= RECORD_COUNTDOWN_INTERVAL_MS) {
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
    bool page_changed = (s_current_page != s_previous_page);

    if (page_changed) {
        // 页面切换：全屏刷新
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
        s_previous_page = s_current_page;
    } else {
        // 同页面内：局部刷新
        switch (s_current_page) {
        case UI_PAGE_HOME:
            update_home_time();
            break;
        case UI_PAGE_RECORD:
            if (s_selected_key != s_last_selected_key) {
                // 类别切换：重绘内容区
                update_record_category();
                s_last_selected_key = s_selected_key;
            } else {
                // 仅更新倒计时
                update_record_countdown();
            }
            break;
        case UI_PAGE_LIST:
            if (s_list_current_index != s_last_list_index) {
                // 翻页：更新序号 + 内容区
                update_list_item();
                s_last_list_index = s_list_current_index;
            } else {
                // 仅更新倒计时
                update_list_countdown();
            }
            break;
        default:
            break;
        }
    }
}

// ============ 页面绘制函数 ============

static void draw_home_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    // 状态栏：品牌居左 + 时间居右
    char time_buf[8];
    svc_time_format_time(svc_time_get_timestamp(), time_buf);
    drv_st7789_fb_draw_string(SAFE_LEFT, SAFE_TOP, "Cloudy", COLOR_BLUE, COLOR_WHITE);
    drv_st7789_fb_draw_string(SAFE_RIGHT - 40, SAFE_TOP, time_buf, COLOR_BLACK, COLOR_WHITE);

    // 主内容：统计信息（居中）
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

    drv_st7789_fb_draw_string_center_1_5x(95, line1, COLOR_BLACK, COLOR_WHITE);
    drv_st7789_fb_draw_string_center_1_5x(125, line2, COLOR_BLACK, COLOR_WHITE);

    // 提示栏（底部居中）
    drv_st7789_fb_draw_string(center_x(20, 1), SAFE_BOTTOM - 16, "K1-K7: Add  K8: List", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();
}

static void draw_record_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    // 获取类别配置
    const food_category_config_t *config = food_category_from_key(s_selected_key);
    if (!config) return;

    // 品牌（左上角）
    drv_st7789_fb_draw_string(SAFE_LEFT, SAFE_TOP, "Cloudy", COLOR_BLUE, COLOR_WHITE);

    // 倒计时（右上角）
    int64_t now = svc_time_get_uptime_ms();
    int64_t remaining_ms = TIMEOUT_RECORD_MS - (now - s_page_enter_time);
    if (remaining_ms < 0) remaining_ms = 0;
    int32_t remaining_sec = (int32_t)(remaining_ms / 1000);
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%3lds", (long)remaining_sec);
    drv_st7789_fb_draw_string(SAFE_RIGHT - 32, SAFE_TOP, countdown, COLOR_RED, COLOR_WHITE);

    // 类别图标（整体垂直居中：安全区域高度248，图标~72+间距8+文字16=96，起始y=16+76=92，图标中心=92+36=108）
    draw_category_icon(config->category, SAFE_CENTER_X, 108);

    // 类别:有效期（图标与底部提示之间居中，1.5倍字体）
    char info[32];
    snprintf(info, sizeof(info), "%s: %d min", config->name, config->default_shelf_life);
    drv_st7789_fb_draw_string_center_1_5x(172, info, COLOR_BLACK, COLOR_WHITE);

    // 提示（底部居中）
    drv_st7789_fb_draw_string(center_x(11, 1), SAFE_BOTTOM - 16, "K8: Confirm", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();

    // 保存状态
    s_last_selected_key = s_selected_key;
}

// ============ 局部更新函数 ============
// 直接用带背景色的文字覆盖旧内容，无需 fill_rect 清除

static void update_home_time(void)
{
    // 固定宽度 5 字符："HH:MM"
    char time_buf[8];
    svc_time_format_time(svc_time_get_timestamp(), time_buf);
    drv_st7789_draw_string(SAFE_RIGHT - 40, SAFE_TOP, time_buf, COLOR_BLACK, COLOR_WHITE);
}

static void update_record_countdown(void)
{
    // 固定宽度 4 字符：" XXs"
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
    // 类别切换内容变化大，重新全屏绘制
    draw_record_page();
}

static void update_list_countdown(void)
{
    // 固定宽度 4 字符：" XXs"
    int64_t now = svc_time_get_uptime_ms();
    int64_t remaining_ms = TIMEOUT_LIST_MS - (now - s_page_enter_time);
    if (remaining_ms < 0) remaining_ms = 0;
    int32_t remaining_sec = (int32_t)(remaining_ms / 1000);
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%3lds", (long)remaining_sec);
    drv_st7789_draw_string(SAFE_RIGHT - 32, SAFE_TOP, countdown, COLOR_RED, COLOR_WHITE);
}

static void update_list_item(void)
{
    // 翻页内容变化大，重新全屏绘制
    draw_list_page();
}

static void draw_list_item_content(uint32_t index)
{
    // 已合并到 draw_list_page，保留空函数
    (void)index;
}

// 列表页：绘制头部序号（固定宽度 15 字符）
static void draw_list_header(void)
{
    uint32_t count = svc_storage_count();
    char header[32];
    snprintf(header, sizeof(header), "List %lu/%lu     ", (unsigned long)(s_list_current_index + 1), (unsigned long)count);
    drv_st7789_draw_string(SAFE_LEFT, SAFE_TOP, header, COLOR_BLUE, COLOR_WHITE);
}

static void draw_list_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    uint32_t count = svc_storage_count();
    if (count == 0) {
        drv_st7789_fb_draw_string_center(120, "No items", COLOR_BLACK, COLOR_WHITE, 2);
        drv_st7789_fb_flush();
        return;
    }

    // 状态栏
    char header[32];
    snprintf(header, sizeof(header), "List %lu/%lu", (unsigned long)(s_list_current_index + 1), (unsigned long)count);
    drv_st7789_fb_draw_string(SAFE_LEFT, SAFE_TOP, header, COLOR_BLUE, COLOR_WHITE);

    // 倒计时（右上角）
    int64_t now = svc_time_get_uptime_ms();
    int64_t remaining_ms = TIMEOUT_LIST_MS - (now - s_page_enter_time);
    if (remaining_ms < 0) remaining_ms = 0;
    int32_t remaining_sec = (int32_t)(remaining_ms / 1000);
    char countdown[8];
    snprintf(countdown, sizeof(countdown), "%3lds", (long)remaining_sec);
    drv_st7789_fb_draw_string(SAFE_RIGHT - 32, SAFE_TOP, countdown, COLOR_RED, COLOR_WHITE);

    // 内容（图标 + 文字）
    uint32_t found_index = 0;
    for (uint32_t id = 1; id <= count + 100; id++) {
        food_record_t record;
        if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
            if (found_index == s_list_current_index) {
                const food_category_config_t *config = food_category_get_config(record.category);
                if (config) {
                    // 图标
                    draw_category_icon(record.category, SAFE_CENTER_X, 108);

                    // 剩余天数（1.5倍字体，黑色）
                    int32_t days = app_food_get_remaining_days(&record);
                    char info[32];
                    if (days < 0) {
                        snprintf(info, sizeof(info), "EXPIRED %ld min", (long)-days);
                    } else if (days == 0) {
                        snprintf(info, sizeof(info), "Expires today!");
                    } else {
                        snprintf(info, sizeof(info), "%ld min left", (long)days);
                    }
                    drv_st7789_fb_draw_string_center_1_5x(172, info, COLOR_BLACK, COLOR_WHITE);
                }
                break;
            }
            found_index++;
        }
    }

    // 提示（底部居中）
    drv_st7789_fb_draw_string(center_x(8, 1), SAFE_BOTTOM - 16, "K8: Back", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();

    // 保存状态
    s_last_list_index = s_list_current_index;
}

static void draw_alert_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    // 状态栏（居中）
    if (s_has_expired) {
        drv_st7789_fb_draw_string(center_x(8, 1), SAFE_TOP, "EXPIRED!", COLOR_RED, COLOR_WHITE);
    } else if (s_has_expiring) {
        drv_st7789_fb_draw_string(center_x(13, 1), SAFE_TOP, "Expiring Soon", COLOR_YELLOW, COLOR_WHITE);
    }

    // 统计信息（居中）
    food_stats_t stats;
    app_food_get_stats(&stats);

    if (stats.expired_count > 0) {
        char line1[32];
        snprintf(line1, sizeof(line1), "%lu items EXPIRED", (unsigned long)stats.expired_count);
        drv_st7789_fb_draw_string(center_x(strlen(line1), 1), 100, line1, COLOR_RED, COLOR_WHITE);
    }

    if (stats.expiring_count > 0) {
        char line2[32];
        snprintf(line2, sizeof(line2), "%lu items expiring", (unsigned long)stats.expiring_count);
        drv_st7789_fb_draw_string(center_x(strlen(line2), 1), 130, line2, COLOR_YELLOW, COLOR_WHITE);
    }

    // 提示（底部居中）
    drv_st7789_fb_draw_string(center_x(11, 1), SAFE_BOTTOM - 16, "K8: Dismiss", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();
}

static void draw_config_page(void)
{
    drv_st7789_fb_clear(COLOR_WHITE);

    // 状态栏（居中）
    drv_st7789_fb_draw_string(center_x(10, 1), SAFE_TOP, "BLE Config", COLOR_BLUE, COLOR_WHITE);

    // 主内容（居中）
    drv_st7789_fb_draw_string(center_x(11, 1), 90, "Waiting for", COLOR_BLACK, COLOR_WHITE);
    drv_st7789_fb_draw_string(center_x(12, 1), 120, "connection...", COLOR_BLACK, COLOR_WHITE);

    // 设备ID（居中）
    drv_st7789_fb_draw_string(center_x(10, 1), 160, "ID: YX-001", COLOR_BLACK, COLOR_WHITE);

    // 提示（底部居中）
    drv_st7789_fb_draw_string(center_x(18, 1), SAFE_BOTTOM - 16, "Hold K8 3s to exit", COLOR_GRAY, COLOR_WHITE);

    drv_st7789_fb_flush();
}
