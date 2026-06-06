// App-Cloudy/product/app_food.h
// 食材管理模块 — 业务逻辑层
// 参考：C2_硬件架构文档.md 6.4节

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "food_category.h"
#include "svc_storage.h"

#ifdef __cplusplus
extern "C" {
#endif

// 食材过期状态
typedef enum {
    FOOD_STATUS_FRESH = 0,     // 新鲜（距过期 > 2天）
    FOOD_STATUS_EXPIRING,      // 临期（0 < 距过期 ≤ 2天）
    FOOD_STATUS_EXPIRED,       // 已过期（距过期 ≤ 0天）
} food_status_t;

// 食材统计信息
typedef struct {
    uint32_t total_count;       // 总数
    uint32_t expiring_count;    // 临期数量
    uint32_t expired_count;     // 过期数量
} food_stats_t;

/**
 * @brief 初始化食材管理模块
 * @return 0=成功, -1=失败
 */
int app_food_init(void);

/**
 * @brief 录入食材（按键触发）
 * @param key_id 按键编号 (1-7对应K1-K7)
 * @param[out] record 输出录入的记录
 * @return 0=成功, -1=失败
 */
int app_food_record(uint8_t key_id, food_record_t *record);

/**
 * @brief 获取食材统计信息
 * @param[out] stats 输出统计
 */
void app_food_get_stats(food_stats_t *stats);

/**
 * @brief 获取指定食材的过期状态
 * @param record 食材记录
 * @return 过期状态
 */
food_status_t app_food_get_status(const food_record_t *record);

/**
 * @brief 获取食材剩余天数
 * @param record 食材记录
 * @return 剩余天数（负数表示已过期天数）
 */
int32_t app_food_get_remaining_days(const food_record_t *record);

/**
 * @brief 检查并执行自动延期
 * @param record_id 记录ID
 * @param record 食材记录
 * @return true=已延期, false=未延期
 */
bool app_food_try_extend(uint32_t record_id, food_record_t *record);

/**
 * @brief 执行过期检查（定时器触发）
 *        遍历所有记录，检查过期状态，执行自动延期
 */
void app_food_check_expiry(void);

#ifdef __cplusplus
}
#endif
