// App-Cloudy/service/svc_storage.c
// NVS 存储服务实现 — 使用 ESP-IDF NVS API

#include "svc_storage.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "svc_storage";

// NVS 命名空间
#define NVS_NAMESPACE       "food_store"

// NVS 键名前缀和计数键
#define NVS_KEY_PREFIX      "food_"
#define NVS_KEY_COUNT       "food_count"
#define NVS_KEY_NEXT_ID     "next_id"

// 最大食材记录数（NVS容量限制，24KB分区，每条约60B含头部）
#define MAX_FOOD_RECORDS    400

// NVS 句柄
static nvs_handle_t s_nvs_handle = 0;
static uint32_t s_record_count = 0;
static uint32_t s_next_id = 1;

// 生成记录键名
static void make_key(uint32_t id, char *buf, size_t buf_len)
{
    snprintf(buf, buf_len, "%s%lu", NVS_KEY_PREFIX, (unsigned long)id);
}

svc_storage_err_t svc_storage_init(void)
{
    esp_err_t err;

    // 初始化 NVS Flash
    err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS partition truncated, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS flash init failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_INIT;
    }

    // 打开命名空间
    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS open failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_INIT;
    }

    // 读取记录计数
    err = nvs_get_u32(s_nvs_handle, NVS_KEY_COUNT, &s_record_count);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_record_count = 0;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS read count failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_READ;
    }

    // 读取下一个可用ID
    err = nvs_get_u32(s_nvs_handle, NVS_KEY_NEXT_ID, &s_next_id);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        s_next_id = 1;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS read next_id failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_READ;
    }

    ESP_LOGI(TAG, "NVS init OK, count=%lu, next_id=%lu",
             (unsigned long)s_record_count, (unsigned long)s_next_id);
    return SVC_STORAGE_OK;
}

svc_storage_err_t svc_storage_add(const food_record_t *record, uint32_t *record_id)
{
    if (s_record_count >= MAX_FOOD_RECORDS) {
        ESP_LOGW(TAG, "Storage full (%lu/%d)", (unsigned long)s_record_count, MAX_FOOD_RECORDS);
        return SVC_STORAGE_ERR_FULL;
    }

    char key[16];
    uint32_t id = s_next_id;
    make_key(id, key, sizeof(key));

    esp_err_t err = nvs_set_blob(s_nvs_handle, key, record, sizeof(food_record_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS write failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_WRITE;
    }

    // 更新计数和下一个ID
    s_record_count++;
    s_next_id++;

    nvs_set_u32(s_nvs_handle, NVS_KEY_COUNT, s_record_count);
    nvs_set_u32(s_nvs_handle, NVS_KEY_NEXT_ID, s_next_id);
    nvs_commit(s_nvs_handle);

    if (record_id) {
        *record_id = id;
    }

    ESP_LOGI(TAG, "Added record id=%lu, category=%d, count=%lu",
             (unsigned long)id, record->category, (unsigned long)s_record_count);
    return SVC_STORAGE_OK;
}

svc_storage_err_t svc_storage_get(uint32_t record_id, food_record_t *record)
{
    char key[16];
    make_key(record_id, key, sizeof(key));

    size_t len = sizeof(food_record_t);
    esp_err_t err = nvs_get_blob(s_nvs_handle, key, record, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return SVC_STORAGE_ERR_NOT_FOUND;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS read failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_READ;
    }

    return SVC_STORAGE_OK;
}

svc_storage_err_t svc_storage_update(uint32_t record_id, const food_record_t *record)
{
    char key[16];
    make_key(record_id, key, sizeof(key));

    // 检查记录是否存在
    food_record_t dummy;
    size_t len = sizeof(food_record_t);
    esp_err_t err = nvs_get_blob(s_nvs_handle, key, &dummy, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return SVC_STORAGE_ERR_NOT_FOUND;
    }

    // 写入更新
    err = nvs_set_blob(s_nvs_handle, key, record, sizeof(food_record_t));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS update failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_WRITE;
    }

    nvs_commit(s_nvs_handle);
    ESP_LOGI(TAG, "Updated record id=%lu", (unsigned long)record_id);
    return SVC_STORAGE_OK;
}

svc_storage_err_t svc_storage_delete(uint32_t record_id)
{
    char key[16];
    make_key(record_id, key, sizeof(key));

    esp_err_t err = nvs_erase_key(s_nvs_handle, key);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        return SVC_STORAGE_ERR_NOT_FOUND;
    } else if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS delete failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_WRITE;
    }

    if (s_record_count > 0) {
        s_record_count--;
    }
    nvs_set_u32(s_nvs_handle, NVS_KEY_COUNT, s_record_count);
    nvs_commit(s_nvs_handle);

    ESP_LOGI(TAG, "Deleted record id=%lu, count=%lu",
             (unsigned long)record_id, (unsigned long)s_record_count);
    return SVC_STORAGE_OK;
}

uint32_t svc_storage_count(void)
{
    return s_record_count;
}

void svc_storage_foreach(void (*callback)(uint32_t id, const food_record_t *record, void *user_data),
                         void *user_data)
{
    if (!callback) return;

    // 遍历所有可能的ID（从1到next_id-1）
    food_record_t record;
    for (uint32_t id = 1; id < s_next_id; id++) {
        if (svc_storage_get(id, &record) == SVC_STORAGE_OK) {
            callback(id, &record, user_data);
        }
    }
}

svc_storage_err_t svc_storage_clear(void)
{
    // 关闭并重新打开以清除所有键
    nvs_close(s_nvs_handle);

    esp_err_t err = nvs_flash_erase();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS erase failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_WRITE;
    }

    err = nvs_flash_init();
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS reinit failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_INIT;
    }

    err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &s_nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "NVS reopen failed: %s", esp_err_to_name(err));
        return SVC_STORAGE_ERR_INIT;
    }

    s_record_count = 0;
    s_next_id = 1;

    ESP_LOGI(TAG, "Storage cleared");
    return SVC_STORAGE_OK;
}
