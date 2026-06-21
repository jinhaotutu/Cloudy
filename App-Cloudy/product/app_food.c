// App-Cloudy/product/app_food.c
// 食材管理模块实现

#include "app_food.h"
#include "svc_time.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "app_food";

int app_food_init(void)
{
    ESP_LOGI(TAG, "Food module init");
    // 存储服务已在 svc_storage_init() 中初始化
    // 此处可添加额外初始化逻辑
    return 0;
}

int app_food_record(uint8_t key_id, food_record_t *record)
{
    const food_category_config_t *config = food_category_from_key(key_id);
    if (!config) {
        ESP_LOGE(TAG, "Invalid key_id: %d", key_id);
        return -1;
    }

    // 构建食材记录
    food_record_t new_record;
    memset(&new_record, 0, sizeof(new_record));

    new_record.category = config->category;
    new_record.extended = 0;
    new_record.shelf_life = config->default_shelf_life;
    new_record.record_time = svc_time_get_timestamp();
    new_record.expiry_time = new_record.record_time + (int64_t)config->default_shelf_life * 86400LL;
    new_record.countdown_sec = (int32_t)config->default_shelf_life * 86400;  // 初始剩余秒数

    // 写入NVS
    uint32_t record_id;
    svc_storage_err_t err = svc_storage_add(&new_record, &record_id);
    if (err != SVC_STORAGE_OK) {
        ESP_LOGE(TAG, "Storage add failed: %d", err);
        return -1;
    }

    if (record) {
        memcpy(record, &new_record, sizeof(food_record_t));
    }

    ESP_LOGI(TAG, "Recorded: cat=%s, shelf_life=%d days, id=%lu",
             config->name, config->default_shelf_life, (unsigned long)record_id);
    return 0;
}

void app_food_get_stats(food_stats_t *stats)
{
    memset(stats, 0, sizeof(food_stats_t));

    // 遍历所有记录统计
    stats->total_count = svc_storage_count();

    // 需要遍历获取临期和过期数量
    // 使用静态变量存储最新统计（简化实现）
    static food_stats_t s_cached_stats = {0};
    static int64_t s_last_check_time = 0;

    int64_t now = svc_time_get_timestamp();
    // 每10秒更新一次缓存
    if (now - s_last_check_time > 10 || s_cached_stats.total_count != stats->total_count) {
        s_cached_stats.total_count = stats->total_count;
        s_cached_stats.expiring_count = 0;
        s_cached_stats.expired_count = 0;

        // 遍历所有记录
        for (uint32_t id = 1; id <= stats->total_count + 100; id++) {
            food_record_t record;
            if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
                food_status_t status = app_food_get_status(&record);
                if (status == FOOD_STATUS_EXPIRING) {
                    s_cached_stats.expiring_count++;
                } else if (status == FOOD_STATUS_EXPIRED) {
                    s_cached_stats.expired_count++;
                }
            }
        }
        s_last_check_time = now;
    }

    stats->expiring_count = s_cached_stats.expiring_count;
    stats->expired_count = s_cached_stats.expired_count;
}

food_status_t app_food_get_status(const food_record_t *record)
{
    if (!record) return FOOD_STATUS_FRESH;

    int64_t now = svc_time_get_timestamp();
    int64_t time_to_expiry = record->expiry_time - now;

    if (time_to_expiry <= 0) {
        return FOOD_STATUS_EXPIRED;
    } else if (time_to_expiry <= 2 * 86400LL) {
        return FOOD_STATUS_EXPIRING;
    }

    return FOOD_STATUS_FRESH;
}

int32_t app_food_get_remaining_days(const food_record_t *record)
{
    if (!record) return 0;

    if (svc_time_is_synced()) {
        int64_t now = svc_time_get_timestamp();
        int64_t time_to_expiry = record->expiry_time - now;
        return (int32_t)(time_to_expiry / 86400LL);
    }

    // NTP 未同步：使用 countdown_sec
    return record->countdown_sec / 86400;
}

int32_t app_food_get_remaining_sec(const food_record_t *record)
{
    if (!record) return 0;

    if (svc_time_is_synced()) {
        int64_t now = svc_time_get_timestamp();
        return (int32_t)(record->expiry_time - now);
    }

    return record->countdown_sec;
}

void app_food_sync_countdown(void)
{
    if (!svc_time_is_synced()) return;

    int64_t now = svc_time_get_timestamp();
    uint32_t count = svc_storage_count();
    uint32_t updated = 0;

    for (uint32_t id = 1; id <= count + 100; id++) {
        food_record_t record;
        if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
            int32_t remaining = (int32_t)(record.expiry_time - now);
            if (record.countdown_sec != remaining) {
                record.countdown_sec = remaining;
                svc_storage_update(id, &record);
                updated++;
            }
        }
    }

    if (updated > 0) {
        ESP_LOGI(TAG, "Synced countdown for %lu records", (unsigned long)updated);
    }
}

bool app_food_try_extend(uint32_t record_id, food_record_t *record)
{
    if (!record || record->extended) {
        return false;
    }

    // 只有已过期的食材才自动延期
    food_status_t status = app_food_get_status(record);
    if (status != FOOD_STATUS_EXPIRED) {
        return false;
    }

    // 获取类别配置
    const food_category_config_t *config = food_category_get_config(record->category);
    if (!config) {
        return false;
    }

    // 执行延期
    int64_t now = svc_time_get_timestamp();
    record->expiry_time = now + (int64_t)config->auto_extend_days * 86400LL;
    record->extended = 1;

    // 更新存储
    svc_storage_err_t err = svc_storage_update(record_id, record);
    if (err != SVC_STORAGE_OK) {
        ESP_LOGE(TAG, "Extend update failed: %d", err);
        return false;
    }

    ESP_LOGI(TAG, "Extended record id=%lu by %d days",
             (unsigned long)record_id, config->auto_extend_days);
    return true;
}

void app_food_check_expiry(void)
{
    ESP_LOGI(TAG, "Checking expiry...");

    // 遍历所有记录，检查过期状态
    uint32_t count = svc_storage_count();
    uint32_t expired_count = 0;
    uint32_t expiring_count = 0;

    for (uint32_t id = 1; id <= count + 100; id++) {
        food_record_t record;
        if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
            // 尝试自动延期
            app_food_try_extend(id, &record);

            // 统计状态
            food_status_t status = app_food_get_status(&record);
            if (status == FOOD_STATUS_EXPIRED) {
                expired_count++;
            } else if (status == FOOD_STATUS_EXPIRING) {
                expiring_count++;
            }
        }
    }

    ESP_LOGI(TAG, "Expiry check done: total=%lu, expiring=%lu, expired=%lu",
             (unsigned long)count, (unsigned long)expiring_count, (unsigned long)expired_count);
}

int64_t app_food_get_next_event_seconds(void)
{
    int64_t now = svc_time_get_timestamp();
    int64_t nearest = -1;  // -1 表示无待处理事件

    uint32_t count = svc_storage_count();
    if (count == 0) return -1;

    for (uint32_t id = 1; id <= count + 100; id++) {
        food_record_t record;
        if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
            int64_t time_to_expiry = record.expiry_time - now;

            // 已过期 → 需要立即处理
            if (time_to_expiry <= 0) {
                return 0;
            }

            // 距临期时间（过期前2天）
            int64_t time_to_expiring = time_to_expiry - 2 * 86400LL;
            if (time_to_expiring <= 0) {
                // 已经在临期区间，检查是否需要变为临期状态
                // 但还没过期，等过期时间到了再触发
                if (nearest < 0 || time_to_expiry < nearest) {
                    nearest = time_to_expiry;
                }
            } else {
                // 还是 FRESH，等临期时间到
                if (nearest < 0 || time_to_expiring < nearest) {
                    nearest = time_to_expiring;
                }
            }
        }
    }

    return nearest;
}

// ==================== Phase 2: MQTT 远程接口 ====================

uint32_t app_food_get_all(food_record_t *records, uint32_t max_count)
{
    if (!records || max_count == 0) return 0;

    uint32_t count = svc_storage_count();
    uint32_t collected = 0;

    for (uint32_t id = 1; id <= count + 100 && collected < max_count; id++) {
        if (svc_storage_get(id, &records[collected]) == SVC_STORAGE_OK) {
            collected++;
        }
    }

    ESP_LOGI(TAG, "get_all: collected %lu records", (unsigned long)collected);
    return collected;
}

uint32_t app_food_get_sorted(food_record_t *records, uint32_t *ids, uint32_t max_count)
{
    if (!records || max_count == 0) return 0;

    uint32_t total = svc_storage_count();
    uint32_t collected = 0;

    // 收集记录和对应的 NVS ID
    for (uint32_t id = 1; id <= total + 100 && collected < max_count; id++) {
        if (svc_storage_get(id, &records[collected]) == SVC_STORAGE_OK) {
            if (ids) ids[collected] = id;
            collected++;
        }
    }

    if (collected <= 1) return collected;

    // 冒泡排序：按 expiry_time 升序，同时交换 ID
    for (uint32_t i = 0; i < collected - 1; i++) {
        for (uint32_t j = 0; j < collected - 1 - i; j++) {
            if (records[j].expiry_time > records[j + 1].expiry_time) {
                food_record_t tmp_rec = records[j];
                records[j] = records[j + 1];
                records[j + 1] = tmp_rec;
                if (ids) {
                    uint32_t tmp_id = ids[j];
                    ids[j] = ids[j + 1];
                    ids[j + 1] = tmp_id;
                }
            }
        }
    }

    return collected;
}

int app_food_delete_by_index(uint32_t index)
{
    uint32_t count = svc_storage_count();
    uint32_t pos = 0;

    for (uint32_t id = 1; id <= count + 100; id++) {
        food_record_t dummy;
        if (svc_storage_get(id, &dummy) == SVC_STORAGE_OK) {
            if (pos == index) {
                svc_storage_err_t err = svc_storage_delete(id);
                if (err != SVC_STORAGE_OK) {
                    ESP_LOGE(TAG, "delete_by_index failed: id=%lu err=%d", (unsigned long)id, err);
                    return -1;
                }
                ESP_LOGI(TAG, "Deleted index=%lu (id=%lu)", (unsigned long)index, (unsigned long)id);
                return 0;
            }
            pos++;
        }
    }

    ESP_LOGW(TAG, "delete_by_index: index %lu not found", (unsigned long)index);
    return -1;
}

int app_food_delete_by_nvs_id(uint32_t nvs_id)
{
    svc_storage_err_t err = svc_storage_delete(nvs_id);
    if (err != SVC_STORAGE_OK) {
        ESP_LOGE(TAG, "delete_by_nvs_id failed: id=%lu err=%d", (unsigned long)nvs_id, err);
        return -1;
    }
    ESP_LOGI(TAG, "Deleted nvs_id=%lu", (unsigned long)nvs_id);
    return 0;
}

int app_food_clear_all(void)
{
    svc_storage_err_t err = svc_storage_clear();
    if (err != SVC_STORAGE_OK) {
        ESP_LOGE(TAG, "clear_all failed: %d", err);
        return -1;
    }

    ESP_LOGI(TAG, "All food records cleared");
    return 0;
}
