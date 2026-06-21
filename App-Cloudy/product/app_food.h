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
 * @brief 获取食材剩余秒数（NTP 未同步时使用 countdown_sec）
 * @param record 食材记录
 * @return 剩余秒数（负数表示已过期）
 */
int32_t app_food_get_remaining_sec(const food_record_t *record);

/**
 * @brief 同步所有食材的倒计时（NTP 同步后调用）
 */
void app_food_sync_countdown(void);

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

/**
 * @brief 获取距最近一个状态变化的秒数
 *        即最近一个食材从 FRESH→EXPIRING 或 EXPIRING→EXPIRED 的时间
 * @return 秒数，无食材时返回 -1，已有过期物品时返回 0
 */
int64_t app_food_get_next_event_seconds(void);

// ==================== Phase 2: MQTT 远程接口 ====================

/**
 * @brief 获取所有食材记录（用于远程查询）
 * @param[out] records 输出数组（调用者分配）
 * @param max_count 数组最大容量
 * @return 实际记录数量
 */
uint32_t app_food_get_all(food_record_t *records, uint32_t max_count);

/**
 * @brief 获取按过期时间排序的食材记录（最近过期排最前）
 * @param[out] records 输出数组（调用者分配）
 * @param[out] ids 输出对应的 NVS 记录 ID（可为 NULL）
 * @param max_count 数组最大容量
 * @return 实际记录数量
 */
uint32_t app_food_get_sorted(food_record_t *records, uint32_t *ids, uint32_t max_count);

/**
 * @brief 按索引删除食材（用于远程删除）
 *        索引为遍历顺序（0-based），非 record_id
 * @param index 要删除的索引（0-based）
 * @return 0=成功, -1=失败
 */
int app_food_delete_by_index(uint32_t index);

/**
 * @brief 按 NVS 记录 ID 删除食材
 * @param nvs_id NVS 记录 ID
 * @return 0=成功, -1=失败
 */
int app_food_delete_by_nvs_id(uint32_t nvs_id);

/**
 * @brief 清空所有食材（用于远程清空）
 * @return 0=成功, -1=失败
 */
int app_food_clear_all(void);

#ifdef __cplusplus
}
#endif
