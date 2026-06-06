// App-Cloudy/service/svc_time.c
// 时间管理服务实现 — 原型阶段使用开机相对时间

#include "svc_time.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <stdio.h>
#include <inttypes.h>

static const char *TAG = "svc_time";

// 开机时间基准（微秒）
static int64_t s_boot_time_us = 0;

void svc_time_init(void)
{
    s_boot_time_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Time service init, boot_time=%lld us", s_boot_time_us);
}

int64_t svc_time_get_uptime_s(void)
{
    int64_t now_us = esp_timer_get_time();
    return (now_us - s_boot_time_us) / 1000000LL;
}

int64_t svc_time_get_uptime_ms(void)
{
    int64_t now_us = esp_timer_get_time();
    return (now_us - s_boot_time_us) / 1000LL;
}

int64_t svc_time_get_timestamp(void)
{
    // 原型阶段：返回开机相对秒数
    // Phase 2：联网后返回NTP同步的Unix时间戳
    return svc_time_get_uptime_s();
}

int32_t svc_time_diff_days(int64_t t1, int64_t t2)
{
    int64_t diff_seconds = t1 - t2;
    return (int32_t)(diff_seconds / 86400LL);
}

void svc_time_format_date(int64_t timestamp, char *buf)
{
    // 原型阶段：显示"第N天"
    int32_t days = (int32_t)(timestamp / 86400LL);
    snprintf(buf, 6, "D%03d", (int)days);
}

void svc_time_format_time(int64_t timestamp, char *buf)
{
    // 计算时分秒
    int32_t seconds_in_day = (int32_t)(timestamp % 86400LL);
    int32_t hours = seconds_in_day / 3600;
    int32_t minutes = (seconds_in_day % 3600) / 60;
    snprintf(buf, 8, "%02d:%02d", (int)hours, (int)minutes);
}
