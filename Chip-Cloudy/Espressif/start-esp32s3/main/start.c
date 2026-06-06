// Chip-Cloudy/Espressif/start-esp32s3/main/start.c
// 程序入口 — 初始化驱动并创建 FreeRTOS 任务

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_log.h"
#include "esp_system.h"

// 驱动头文件
#include "drv_matrix_key.h"
#include "drv_st7789.h"
#include "drv_led.h"

static const char *TAG = "cloud-start";

// 按键事件队列
static QueueHandle_t s_key_event_queue = NULL;

// 主任务
static void main_task(void *arg)
{
    key_event_t event;

    while (1) {
        // 检查按键事件
        if (xQueueReceive(s_key_event_queue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            ESP_LOGI(TAG, "Key event: id=%d (%s), type=%d, duration=%lums",
                     event.key_id,
                     drv_matrix_key_get_name(event.key_id),
                     event.event,
                     event.duration_ms);

            // 短按确认键：切换 LED
            if (event.key_id == KEY_ID_K8 && event.event == KEY_EVENT_SHORT_PRESS) {
                drv_led_toggle();
                ESP_LOGI(TAG, "LED toggled");
            }
        }
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "Cloud Start - Driver Init");

    // 1. 初始化 LED 驱动
    drv_led_init();

    // 2. 初始化矩阵键盘驱动
    s_key_event_queue = xQueueCreate(16, sizeof(key_event_t));
    drv_matrix_key_init();
    drv_matrix_key_start(s_key_event_queue);

    // 3. 初始化 TFT 屏幕驱动
    drv_st7789_init();
    drv_st7789_fill_screen(COLOR_WHITE);
    drv_st7789_draw_string_center(40, "Cloud Start!", COLOR_BLACK, COLOR_WHITE, 2);
    drv_st7789_draw_string_center(80, "Drivers OK", COLOR_RED, COLOR_WHITE, 2);

    // 4. 启动主任务
    xTaskCreate(main_task, "main_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "All drivers initialized. System running.");

    // LED 闪烁 3 次表示初始化完成
    for (int i = 0; i < 3; i++) {
        drv_led_on();
        vTaskDelay(pdMS_TO_TICKS(200));
        drv_led_off();
        vTaskDelay(pdMS_TO_TICKS(200));
    }
}
