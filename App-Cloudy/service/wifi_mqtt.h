// App-Cloudy/service/wifi_mqtt.h
// Wi-Fi STA + MQTT 客户端模块（非阻塞后台连接）

#pragma once

#include "esp_err.h"
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// MQTT 命令回调函数类型
// 参数: (json_data, data_len)
typedef void (*mqtt_cmd_callback_t)(const char *data, int len);

/**
 * @brief 初始化 Wi-Fi STA 模式（非阻塞，后台自动连接和重连）
 * @param ssid Wi-Fi SSID
 * @param password Wi-Fi 密码
 * @return ESP_OK 成功启动
 */
esp_err_t wifi_init_sta(const char *ssid, const char *password);

/**
 * @brief 设置 MQTT 配置（Wi-Fi 连接后自动启动 MQTT）
 * @param broker_uri Broker 地址 (mqtt://host:port)
 * @param client_id 客户端ID
 * @param username 用户名
 * @param password 密码
 */
void wifi_set_mqtt_config(const char *broker_uri, const char *client_id,
                          const char *username, const char *password);

/**
 * @brief 检查 Wi-Fi 是否已连接
 * @return true 已连接
 */
bool wifi_is_connected(void);

/**
 * @brief 初始化 MQTT 客户端（内部保存配置，Wi-Fi 连接后自动启动）
 * @param broker_uri Broker 地址 (mqtt://host:port)
 * @param client_id 客户端ID
 * @param username 用户名
 * @param password 密码
 * @return ESP_OK 成功
 */
esp_err_t mqtt_app_init(const char *broker_uri, const char *client_id,
                        const char *username, const char *password);

/**
 * @brief 发布主动事件到 MQTT (yunxia/{id}/event)
 *        用于 add/delete/expiry_alert 等设备主动上报
 * @param device_id 设备ID
 * @param json_payload JSON 字符串
 * @return ESP_OK 成功
 */
esp_err_t mqtt_publish_event(const char *device_id, const char *json_payload);

/**
 * @brief 发布命令回复到 MQTT (yunxia/{id}/response)
 *        用于 query_result/delete_result/clear_result 等命令响应
 * @param device_id 设备ID
 * @param json_payload JSON 字符串
 * @return ESP_OK 成功
 */
esp_err_t mqtt_publish_response(const char *device_id, const char *json_payload);

/**
 * @brief 发布状态消息到 MQTT
 * @param device_id 设备ID
 * @param status 状态字符串 (如 "online")
 * @return ESP_OK 成功
 */
esp_err_t mqtt_publish_status(const char *device_id, const char *status);

/**
 * @brief 检查 MQTT 是否已连接
 * @return true 已连接
 */
bool mqtt_is_connected(void);

/**
 * @brief 注册 MQTT 命令处理回调
 * @param callback 回调函数
 */
void mqtt_register_cmd_callback(mqtt_cmd_callback_t callback);

#ifdef __cplusplus
}
#endif
