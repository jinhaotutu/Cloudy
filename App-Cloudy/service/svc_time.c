// App-Cloudy/service/svc_time.c
// 时间管理服务实现 — NTP 校准 + 系统时间维护

#include "svc_time.h"
#include "esp_timer.h"
#include "esp_log.h"
#include "esp_netif_sntp.h"
#include <stdio.h>
#include <inttypes.h>
#include <sys/time.h>

static const char *TAG = "svc_time";

// 开机时间基准（微秒）
static int64_t s_boot_time_us = 0;

// NTP 同步状态
static bool s_ntp_synced = false;
static int64_t s_sync_timestamp = 0;   // 同步时刻的 Unix 时间戳
static int64_t s_sync_uptime_ms = 0;   // 同步时刻的开机毫秒数

// NTP 配置（CONFIG_LWIP_SNTP_MAX_SERVERS=1，只能配1个服务器）
#define NTP_SERVER  "ntp.aliyun.com"

// 中国时区
#define TIMEZONE_CN  "CST-8"

// ==================== 基础时间函数（回调依赖，放前面） ====================

static int64_t svc_time_get_uptime_ms_internal(void)
{
    int64_t now_us = esp_timer_get_time();
    return (now_us - s_boot_time_us) / 1000LL;
}

// ==================== NTP 同步回调 ====================

static void ntp_time_sync_cb(struct timeval *tv)
{
    if (!s_ntp_synced) {
        s_ntp_synced = true;
        s_sync_timestamp = tv->tv_sec;
        s_sync_uptime_ms = svc_time_get_uptime_ms_internal();
        ESP_LOGI(TAG, "NTP synced! Unix time: %lld", (long long)tv->tv_sec);

        // 打印当前北京时间
        setenv("TZ", TIMEZONE_CN, 1);
        tzset();
        time_t now = tv->tv_sec;
        struct tm timeinfo;
        localtime_r(&now, &timeinfo);
        ESP_LOGI(TAG, "Beijing time: %04d-%02d-%02d %02d:%02d:%02d",
                 timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday,
                 timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec);
    }
}

// ==================== 公开 API ====================

void svc_time_init(void)
{
    s_boot_time_us = esp_timer_get_time();
    s_ntp_synced = false;

    // 预设中国时区
    setenv("TZ", TIMEZONE_CN, 1);
    tzset();

    ESP_LOGI(TAG, "Time service init, boot_time=%lld us", (long long)s_boot_time_us);
}

void svc_time_start_ntp(void)
{
    ESP_LOGI(TAG, "Starting NTP sync...");

    esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(NTP_SERVER);
    config.sync_cb = ntp_time_sync_cb;

    esp_netif_sntp_init(&config);
}

bool svc_time_is_synced(void)
{
    return s_ntp_synced;
}

int64_t svc_time_get_uptime_s(void)
{
    int64_t now_us = esp_timer_get_time();
    return (now_us - s_boot_time_us) / 1000000LL;
}

int64_t svc_time_get_uptime_ms(void)
{
    return svc_time_get_uptime_ms_internal();
}

int64_t svc_time_get_timestamp(void)
{
    if (s_ntp_synced) {
        // 基于同步时刻的 Unix 时间 + 经过的毫秒数推算
        int64_t elapsed_ms = svc_time_get_uptime_ms_internal() - s_sync_uptime_ms;
        return s_sync_timestamp + elapsed_ms / 1000LL;
    }

    // 未同步：返回开机相对秒数
    return svc_time_get_uptime_s();
}

int32_t svc_time_diff_days(int64_t t1, int64_t t2)
{
    int64_t diff_seconds = t1 - t2;
    return (int32_t)(diff_seconds / 86400LL);
}

void svc_time_format_date(int64_t timestamp, char *buf)
{
    if (s_ntp_synced) {
        // 已同步：显示真实日期 MM-DD
        time_t t = (time_t)timestamp;
        struct tm timeinfo;
        localtime_r(&t, &timeinfo);
        unsigned char month = (unsigned char)(timeinfo.tm_mon + 1);
        unsigned char day = (unsigned char)timeinfo.tm_mday;
        snprintf(buf, 6, "%02hhu-%02hhu", month, day);
    } else {
        // 未同步：显示"第N天"
        int32_t days = (int32_t)(timestamp / 86400LL);
        snprintf(buf, 6, "D%03d", (int)days);
    }
}

void svc_time_format_time(int64_t timestamp, char *buf)
{
    if (s_ntp_synced) {
        // 已同步：显示真实时间 HH:MM
        time_t t = (time_t)timestamp;
        struct tm timeinfo;
        localtime_r(&t, &timeinfo);
        snprintf(buf, 8, "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
    } else {
        // 未同步：显示开机后时间
        int32_t seconds_in_day = (int32_t)(timestamp % 86400LL);
        int32_t hours = seconds_in_day / 3600;
        int32_t minutes = (seconds_in_day % 3600) / 60;
        snprintf(buf, 8, "%02d:%02d", (int)hours, (int)minutes);
    }
}
