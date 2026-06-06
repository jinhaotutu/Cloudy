// Adapt-Cloudy/driver/drv_matrix_key.c
// 3×3 矩阵键盘扫描驱动实现
// 扫描方式：逐行拉低 → 读列 → 100ms 去抖 → 事件生成

#include "drv_matrix_key.h"
#include "hal_gpio.h"
#include "pin_config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

static const char *TAG = "drv_matrix_key";

// 行引脚数组
static const int s_row_pins[MATRIX_ROWS] = {
    MATRIX_ROW0_GPIO,
    MATRIX_ROW1_GPIO,
    MATRIX_ROW2_GPIO,
};

// 列引脚数组
static const int s_col_pins[MATRIX_COLS] = {
    MATRIX_COL0_GPIO,
    MATRIX_COL1_GPIO,
    MATRIX_COL2_GPIO,
};

// 按键映射表：key_map[row][col] = key_id
// (ROW2, COL1) = 空位，标记为 KEY_ID_NONE
static const key_id_t s_key_map[MATRIX_ROWS][MATRIX_COLS] = {
    { KEY_ID_K1, KEY_ID_K2, KEY_ID_K3 },  // ROW0
    { KEY_ID_K4, KEY_ID_K5, KEY_ID_K6 },  // ROW1
    { KEY_ID_K7, KEY_ID_NONE, KEY_ID_K8 }, // ROW2 (COL1=空位)
};

// 按键状态跟踪
typedef struct {
    bool     pressed;          // 当前是否按下
    uint32_t press_tick;       // 按下时刻
    uint32_t debounce_tick;    // 去抖计数
    bool     event_sent;       // 去抖完成，PRESS 事件已发送
    bool     long_sent;        // 长按事件已发送（最多一次）
} key_state_t;

static key_state_t s_key_state[KEY_ID_MAX];
static QueueHandle_t s_event_queue = NULL;

// 按键名称表
static const char *s_key_names[KEY_ID_MAX] = {
    "NONE", "乳制品", "肉蛋类", "蔬菜类",
    "水果类", "海鲜类", "饮品类", "冷冻类", "确认",
};

void drv_matrix_key_init(void)
{
    // 行引脚：输出，初始高电平（不扫描）
    for (int i = 0; i < MATRIX_ROWS; i++) {
        hal_gpio_init(s_row_pins[i], HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE);
        hal_gpio_set_level(s_row_pins[i], true);
    }

    // 列引脚：输入，上拉
    for (int i = 0; i < MATRIX_COLS; i++) {
        hal_gpio_init(s_col_pins[i], HAL_GPIO_DIR_INPUT, HAL_GPIO_PULL_UP);
    }

    // 初始化按键状态
    for (int i = 0; i < KEY_ID_MAX; i++) {
        s_key_state[i].pressed = false;
        s_key_state[i].press_tick = 0;
        s_key_state[i].debounce_tick = 0;
        s_key_state[i].event_sent = false;
        s_key_state[i].long_sent = false;
    }

    ESP_LOGI(TAG, "Matrix key initialized (3x3)");
}

// 扫描单个按键状态（返回 true 表示按下）
static bool scan_key(int row, int col)
{
    // 拉低当前行
    hal_gpio_set_level(s_row_pins[row], false);

    // 短暂延时等待电平稳定
    esp_rom_delay_us(2);

    // 读取列线（低电平=按下）
    bool pressed = !hal_gpio_get_level(s_col_pins[col]);

    // 恢复行线高电平
    hal_gpio_set_level(s_row_pins[row], true);

    return pressed;
}

// 计算两个 tick 的差值（毫秒），安全处理溢出回绕
static inline uint32_t tick_diff_ms(uint32_t newer, uint32_t older)
{
    return (uint32_t)(newer - older) * portTICK_PERIOD_MS;
}

// 发送按键事件
static void send_key_event(key_id_t key_id, key_event_type_t event, uint32_t duration_ms)
{
    if (s_event_queue == NULL) return;

    key_event_t evt = {
        .key_id = key_id,
        .event = event,
        .duration_ms = duration_ms,
    };

    xQueueSend(s_event_queue, &evt, 0);
}

// 键盘扫描任务
static void matrix_key_task(void *arg)
{
    TickType_t last_wake = xTaskGetTickCount();

    while (1) {
        uint32_t now_tick = xTaskGetTickCount();

        // 扫描所有按键
        for (int row = 0; row < MATRIX_ROWS; row++) {
            for (int col = 0; col < MATRIX_COLS; col++) {
                key_id_t key_id = s_key_map[row][col];
                if (key_id == KEY_ID_NONE) continue;  // 跳过空位

                bool is_pressed = scan_key(row, col);
                key_state_t *state = &s_key_state[key_id];

                if (is_pressed) {
                    if (!state->pressed) {
                        // 新按下：开始去抖计数
                        state->debounce_tick = now_tick;
                        state->pressed = true;
                        state->event_sent = false;
                    } else {
                        // 持续按下：检查去抖（100ms）
                        uint32_t debounce_ms = tick_diff_ms(now_tick, state->debounce_tick);

                        if (debounce_ms >= 100 && !state->event_sent) {
                            // 去抖完成，发送按下事件
                            state->press_tick = now_tick;
                            send_key_event(key_id, KEY_EVENT_PRESS, 0);
                            state->event_sent = true;
                        }

                        // 去抖后检查长按（>= 5s，按住时立即触发，仅一次）
                        if (state->event_sent && !state->long_sent) {
                            uint32_t held_ms = tick_diff_ms(now_tick, state->press_tick);
                            if (held_ms >= 5000) {
                                send_key_event(key_id, KEY_EVENT_LONG_PRESS, held_ms);
                                state->long_sent = true;
                            }
                        }
                    }
                } else {
                    if (state->pressed && state->event_sent) {
                        // 释放：发送释放事件
                        uint32_t held_ms = tick_diff_ms(now_tick, state->press_tick);
                        send_key_event(key_id, KEY_EVENT_RELEASE, held_ms);

                        // 长按已触发过则不重复，否则补发短按
                        if (!state->long_sent) {
                            send_key_event(key_id, KEY_EVENT_SHORT_PRESS, held_ms);
                        }
                    }
                    state->pressed = false;
                    state->debounce_tick = 0;
                    state->event_sent = false;
                    state->long_sent = false;
                }
            }
        }

        // 10ms 扫描周期
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(10));
    }
}

void drv_matrix_key_start(QueueHandle_t event_queue)
{
    s_event_queue = event_queue;

    xTaskCreate(
        matrix_key_task,
        "key_scan",
        2048,        // 栈大小 2048B（C2 设计）
        NULL,
        6,           // 优先级 6（C2 设计）
        NULL
    );

    ESP_LOGI(TAG, "Key scan task started");
}

const char *drv_matrix_key_get_name(key_id_t key_id)
{
    if (key_id >= KEY_ID_MAX) return "UNKNOWN";
    return s_key_names[key_id];
}
