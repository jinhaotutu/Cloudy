// Adapt-Cloudy/hal/hal_timer.c
// 定时器硬件抽象层实现

#include "hal_timer.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "hal_timer";

hal_timer_handle_t hal_timer_create_once(const char *name, uint32_t timeout_ms,
                                          hal_timer_cb_t callback, void *arg)
{
    esp_timer_handle_t handle = NULL;
    const esp_timer_create_args_t timer_args = {
        .callback = callback,
        .arg = arg,
        .name = name,
        .skip_unhandled_events = false,
    };

    esp_err_t ret = esp_timer_create(&timer_args, &handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_timer_create failed: %s", esp_err_to_name(ret));
        return NULL;
    }

    return (hal_timer_handle_t)handle;
}

hal_timer_handle_t hal_timer_create_periodic(const char *name, uint32_t period_ms,
                                              hal_timer_cb_t callback, void *arg)
{
    // 周期定时器和单次定时器使用相同的创建接口
    // 区别在于启动时调用 esp_timer_start_periodic vs esp_timer_start_once
    return hal_timer_create_once(name, 0, callback, arg);
}

bool hal_timer_start_once(hal_timer_handle_t handle, uint32_t timeout_ms)
{
    esp_err_t ret = esp_timer_start_once((esp_timer_handle_t)handle, timeout_ms * 1000);
    return (ret == ESP_OK);
}

bool hal_timer_start_periodic(hal_timer_handle_t handle, uint32_t period_ms)
{
    esp_err_t ret = esp_timer_start_periodic((esp_timer_handle_t)handle, period_ms * 1000);
    return (ret == ESP_OK);
}

bool hal_timer_stop(hal_timer_handle_t handle)
{
    esp_err_t ret = esp_timer_stop((esp_timer_handle_t)handle);
    return (ret == ESP_OK);
}

void hal_timer_delete(hal_timer_handle_t handle)
{
    if (handle) {
        esp_timer_delete((esp_timer_handle_t)handle);
    }
}

int64_t hal_timer_get_ms(void)
{
    return esp_timer_get_time() / 1000;
}
