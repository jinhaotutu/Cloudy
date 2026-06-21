// App-Cloudy/service/svc_storage.h
// NVS 存储服务 — 食材记录的持久化存储
// 参考：C2_硬件架构文档.md 5.3节

#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 食材记录结构体
typedef struct {
    uint8_t  category;      // 类别编号 1-7
    uint8_t  extended;      // 是否已自动延期 (0=未延期, 1=已延期)
    uint16_t shelf_life;    // 保质期天数
    int64_t  record_time;   // 录入时间戳（Unix 秒）
    int64_t  expiry_time;   // 过期时间戳（Unix 秒）
    int32_t  countdown_sec; // NTP 同步时的剩余秒数（离线兜底）
} food_record_t;            // 共 24 字节

// 存储操作返回码
typedef enum {
    SVC_STORAGE_OK = 0,
    SVC_STORAGE_ERR_INIT,       // 初始化失败
    SVC_STORAGE_ERR_FULL,       // 存储满
    SVC_STORAGE_ERR_NOT_FOUND,  // 记录未找到
    SVC_STORAGE_ERR_WRITE,      // 写入失败
    SVC_STORAGE_ERR_READ,       // 读取失败
} svc_storage_err_t;

/**
 * @brief 初始化 NVS 存储服务
 * @return 错误码
 */
svc_storage_err_t svc_storage_init(void);

/**
 * @brief 添加一条食材记录
 * @param record 食材记录指针
 * @param[out] record_id 输出记录ID
 * @return 错误码
 */
svc_storage_err_t svc_storage_add(const food_record_t *record, uint32_t *record_id);

/**
 * @brief 获取指定ID的食材记录
 * @param record_id 记录ID
 * @param[out] record 输出记录
 * @return 错误码
 */
svc_storage_err_t svc_storage_get(uint32_t record_id, food_record_t *record);

/**
 * @brief 更新指定ID的食材记录
 * @param record_id 记录ID
 * @param record 新记录内容
 * @return 错误码
 */
svc_storage_err_t svc_storage_update(uint32_t record_id, const food_record_t *record);

/**
 * @brief 删除指定ID的食材记录
 * @param record_id 记录ID
 * @return 错误码
 */
svc_storage_err_t svc_storage_delete(uint32_t record_id);

/**
 * @brief 获取当前食材总数
 * @return 食材数量
 */
uint32_t svc_storage_count(void);

/**
 * @brief 遍历所有食材记录
 * @param callback 回调函数，参数为(record_id, record, user_data)
 * @param user_data 用户数据
 */
void svc_storage_foreach(void (*callback)(uint32_t id, const food_record_t *record, void *user_data), void *user_data);

/**
 * @brief 清除所有食材记录
 * @return 错误码
 */
svc_storage_err_t svc_storage_clear(void);

#ifdef __cplusplus
}
#endif
