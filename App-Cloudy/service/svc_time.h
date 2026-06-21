// App-Cloudy/service/svc_time.h
// 时间管理服务 — NTP 校准 + 系统时间维护

#pragma once

#include <stdint.h>
#include <time.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化时间管理服务
 */
void svc_time_init(void);

/**
 * @brief 启动 NTP 时间同步（Wi-Fi 连接后调用）
 *        配置中国时区 UTC+8，使用阿里云 NTP 服务器
 */
void svc_time_start_ntp(void);

/**
 * @brief 检查 NTP 时间是否已同步
 * @return true=已同步
 */
bool svc_time_is_synced(void);

/**
 * @brief 获取开机以来的秒数（相对时间）
 * @return 秒数
 */
int64_t svc_time_get_uptime_s(void);

/**
 * @brief 获取开机以来的毫秒数
 * @return 毫秒数
 */
int64_t svc_time_get_uptime_ms(void);

/**
 * @brief 获取当前时间戳（Unix时间，已同步后返回真实时间）
 * @return Unix时间戳（秒），未同步时返回开机秒数
 */
int64_t svc_time_get_timestamp(void);

/**
 * @brief 计算两个时间戳之间的天数差
 * @param t1 时间戳1（秒）
 * @param t2 时间戳2（秒）
 * @return 天数差（t1 - t2），可为负数
 */
int32_t svc_time_diff_days(int64_t t1, int64_t t2);

/**
 * @brief 将时间戳格式化为日期字符串 MM-DD
 * @param timestamp 时间戳（秒）
 * @param buf 输出缓冲区（至少6字节）
 */
void svc_time_format_date(int64_t timestamp, char *buf);

/**
 * @brief 将时间戳格式化为时间字符串 HH:MM
 * @param timestamp 时间戳（秒）
 * @param buf 输出缓冲区（至少6字节）
 */
void svc_time_format_time(int64_t timestamp, char *buf);

#ifdef __cplusplus
}
#endif
