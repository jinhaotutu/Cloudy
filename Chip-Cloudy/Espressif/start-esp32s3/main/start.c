// Chip-Cloudy/Espressif/start-esp32s3/main/start.c
// 冰箱过期提醒器 — 主程序入口
// 集成所有模块，创建 FreeRTOS 任务，运行主循环

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "esp_log.h"
#include "nvs_flash.h"

// 驱动层
#include "drv_matrix_key.h"
#include "drv_st7789.h"
#include "drv_led.h"

// 服务层
#include "svc_storage.h"
#include "svc_time.h"

// 应用层
#include "app_food.h"
#include "app_ui.h"
#include "app_alert.h"

static const char *TAG = "main";

// 线程间通信
static QueueHandle_t s_key_queue = NULL;
static SemaphoreHandle_t s_expiry_semaphore = NULL;

// 定时器线程 — 每60秒触发过期检查
static void timer_task(void *arg)
{
    SemaphoreHandle_t sem = (SemaphoreHandle_t)arg;

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(60000));
        xSemaphoreGive(sem);
        ESP_LOGI(TAG, "Expiry check triggered");
    }
}

// 主线程 — 事件循环
static void main_task(void *arg)
{
    key_event_t key;
    bool has_expiring, has_expired;

    ESP_LOGI(TAG, "Main task started");

    while (1) {
        // 1. 处理按键事件
        if (xQueueReceive(s_key_queue, &key, pdMS_TO_TICKS(10)) == pdTRUE) {
            app_ui_handle_key(&key);
        }

        // 2. 检查过期信号量
        if (xSemaphoreTake(s_expiry_semaphore, 0) == pdTRUE) {
            app_food_check_expiry();

            // 获取最新统计，更新UI和LED
            food_stats_t stats;
            app_food_get_stats(&stats);
            has_expiring = (stats.expiring_count > 0);
            has_expired = (stats.expired_count > 0);

            app_ui_handle_expiry_check(has_expiring, has_expired);
            app_alert_update(has_expiring, has_expired);
        }

        // 3. UI 更新（超时处理 + 屏幕刷新）
        app_ui_update();

        // 4. LED 闪烁处理
        app_alert_blink_handler();

        // 5. 主循环周期 10ms
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== Cloudy Fridge Reminder ===");

    // 1. 初始化 NVS
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition issue, erasing...");
        nvs_flash_erase();
        nvs_flash_init();
    }

    // 2. 初始化驱动层（硬件先就绪）
    drv_st7789_init();
    drv_led_init();

    // 3. 初始化服务层
    svc_time_init();
    svc_storage_err_t storage_err = svc_storage_init();
    if (storage_err != SVC_STORAGE_OK) {
        ESP_LOGE(TAG, "Storage init failed: %d", storage_err);
    }

    // 4. 初始化应用层（依赖驱动和服务）
    app_food_init();
    app_ui_init();
    app_alert_init();

    // 5. 创建线程间通信对象
    s_key_queue = xQueueCreate(16, sizeof(key_event_t));
    s_expiry_semaphore = xSemaphoreCreateBinary();

    // 6. 启动矩阵键盘扫描（内部创建独立任务）
    drv_matrix_key_init();
    drv_matrix_key_start(s_key_queue);

    // 7. 创建定时器任务（60秒过期检查周期）
    xTaskCreate(timer_task, "timer_task", 2048, s_expiry_semaphore, 4, NULL);

    // 8. 开机首次过期检查
    app_food_check_expiry();
    food_stats_t stats;
    app_food_get_stats(&stats);
    app_ui_handle_expiry_check(stats.expiring_count > 0, stats.expired_count > 0);
    app_alert_update(stats.expiring_count > 0, stats.expired_count > 0);

    // 9. 创建并启动主任务
    xTaskCreate(main_task, "main_task", 4096, NULL, 5, NULL);

    ESP_LOGI(TAG, "System running. Total food: %lu", (unsigned long)stats.total_count);
}
