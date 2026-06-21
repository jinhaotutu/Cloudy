// App-Cloudy/service/wifi_mqtt.c
// Wi-Fi STA + MQTT 客户端实现（非阻塞后台连接）

#include "wifi_mqtt.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_log.h"
#include "mqtt_client.h"
#include <string.h>

static const char *TAG = "wifi_mqtt";

// Wi-Fi 重连参数
#define WIFI_RETRY_MAX      10    // 单轮最大重试次数
#define WIFI_RETRY_DELAY_MS 30000 // 超过后冷却 30 秒再重试

// 静态句柄
static esp_mqtt_client_handle_t s_mqtt_client = NULL;
static bool s_mqtt_connected = false;
static bool s_wifi_connected = false;
static int s_wifi_retry_count = 0;

// 设备 ID
static char s_device_id[32] = {0};

// 命令回调
static mqtt_cmd_callback_t s_cmd_callback = NULL;

// MQTT 配置（用于 Wi-Fi 连接后自动启动）
static char s_mqtt_broker_uri[128] = {0};
static char s_mqtt_client_id[32] = {0};
static char s_mqtt_username[32] = {0};
static char s_mqtt_password[64] = {0};
static bool s_mqtt_configured = false;

// 重连任务句柄
static TaskHandle_t s_reconnect_task_handle = NULL;

// ==================== 前向声明 ====================

static void mqtt_start(void);

// ==================== Wi-Fi 事件处理 ====================

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        s_mqtt_connected = false;

        if (s_wifi_retry_count < WIFI_RETRY_MAX) {
            esp_wifi_connect();
            s_wifi_retry_count++;
            ESP_LOGI(TAG, "Wi-Fi retry %d/%d", s_wifi_retry_count, WIFI_RETRY_MAX);
        } else {
            ESP_LOGW(TAG, "Wi-Fi retry limit reached, cooling down %ds...",
                     WIFI_RETRY_DELAY_MS / 1000);
            // 由重连任务处理后续重连
        }
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
        ESP_LOGI(TAG, "Got IP: " IPSTR, IP2STR(&event->ip_info.ip));
        s_wifi_retry_count = 0;
        s_wifi_connected = true;

        // Wi-Fi 连接成功，自动启动 MQTT
        if (s_mqtt_configured && !s_mqtt_client) {
            mqtt_start();
        }
    }
}

// ==================== 后台重连任务 ====================

static void reconnect_task(void *arg)
{
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(WIFI_RETRY_DELAY_MS));

        if (!s_wifi_connected) {
            ESP_LOGI(TAG, "Wi-Fi reconnecting...");
            s_wifi_retry_count = 0;
            esp_wifi_connect();
        }
    }
}

// ==================== MQTT 事件处理 ====================

static void mqtt_event_handler(void *args, esp_event_base_t base,
                               int32_t event_id, void *event_data)
{
    esp_mqtt_event_handle_t event = event_data;

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        ESP_LOGI(TAG, "MQTT Connected");
        s_mqtt_connected = true;

        // 订阅命令 topic
        if (s_device_id[0] != '\0') {
            char topic_cmd[64];
            snprintf(topic_cmd, sizeof(topic_cmd), "yunxia/%s/cmd", s_device_id);
            esp_mqtt_client_subscribe(s_mqtt_client, topic_cmd, 1);
            ESP_LOGI(TAG, "Subscribed: %s", topic_cmd);

            // 发布上线状态
            mqtt_publish_status(s_device_id, "online");
        }
        break;

    case MQTT_EVENT_DISCONNECTED:
        ESP_LOGW(TAG, "MQTT Disconnected");
        s_mqtt_connected = false;
        break;

    case MQTT_EVENT_DATA:
        ESP_LOGI(TAG, "MQTT Data: topic=%.*s data=%.*s",
                 event->topic_len, event->topic,
                 event->data_len, event->data);

        // 调用注册的命令回调
        if (s_cmd_callback && event->data_len > 0) {
            s_cmd_callback(event->data, event->data_len);
        }
        break;

    case MQTT_EVENT_ERROR:
        ESP_LOGE(TAG, "MQTT Error type=%d", event->error_handle->error_type);
        break;

    default:
        break;
    }
}

// ==================== MQTT 内部启动 ====================

static void mqtt_start(void)
{
    if (s_mqtt_client) {
        return;
    }

    strncpy(s_device_id, s_mqtt_client_id, sizeof(s_device_id) - 1);

    esp_mqtt_client_config_t mqtt_cfg = {
        .broker.address.uri = s_mqtt_broker_uri,
        .credentials.client_id = s_mqtt_client_id,
        .credentials.username = s_mqtt_username,
        .credentials.authentication.password = s_mqtt_password,
        .session.keepalive = 60,
        .session.disable_clean_session = false,
    };

    s_mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
    if (!s_mqtt_client) {
        ESP_LOGE(TAG, "MQTT client init failed");
        return;
    }

    ESP_ERROR_CHECK(esp_mqtt_client_register_event(s_mqtt_client, ESP_EVENT_ANY_ID,
                                                    mqtt_event_handler, NULL));
    ESP_ERROR_CHECK(esp_mqtt_client_start(s_mqtt_client));

    ESP_LOGI(TAG, "MQTT client started: %s", s_mqtt_broker_uri);
}

// ==================== 公开 API ====================

esp_err_t wifi_init_sta(const char *ssid, const char *password)
{
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    // 注册事件处理
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, NULL));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(
        IP_EVENT, IP_EVENT_STA_GOT_IP, &wifi_event_handler, NULL, NULL));

    // 配置 Wi-Fi
    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_WPA2_PSK,
            .sae_pwe_h2e = WPA3_SAE_PWE_BOTH,
            .sae_h2e_identifier = "",
            .scan_method = WIFI_ALL_CHANNEL_SCAN,  // 隐藏 WiFi 需要全信道扫描
        },
    };
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid) - 1);
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password) - 1);

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));

    // 设置国家信息为中国
    wifi_country_t country = {
        .cc = "CN",
        .schan = 1,
        .nchan = 13,
        .policy = WIFI_COUNTRY_POLICY_MANUAL,
    };
    ESP_ERROR_CHECK(esp_wifi_set_country(&country));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_LOGI(TAG, "Wi-Fi STA connecting to: %s", ssid);

    // 启动后台重连任务
    xTaskCreate(reconnect_task, "wifi_reconnect", 2048, NULL, 3, &s_reconnect_task_handle);

    return ESP_OK;
}

void wifi_set_mqtt_config(const char *broker_uri, const char *client_id,
                          const char *username, const char *password)
{
    strncpy(s_mqtt_broker_uri, broker_uri, sizeof(s_mqtt_broker_uri) - 1);
    strncpy(s_mqtt_client_id, client_id, sizeof(s_mqtt_client_id) - 1);
    strncpy(s_mqtt_username, username, sizeof(s_mqtt_username) - 1);
    strncpy(s_mqtt_password, password, sizeof(s_mqtt_password) - 1);
    s_mqtt_configured = true;
}

bool wifi_is_connected(void)
{
    return s_wifi_connected;
}

esp_err_t mqtt_app_init(const char *broker_uri, const char *client_id,
                        const char *username, const char *password)
{
    wifi_set_mqtt_config(broker_uri, client_id, username, password);

    // 如果 Wi-Fi 已连接，立即启动 MQTT
    if (s_wifi_connected) {
        mqtt_start();
    }

    return ESP_OK;
}

esp_err_t mqtt_publish_event(const char *device_id, const char *json_payload)
{
    if (!s_mqtt_client || !s_mqtt_connected) {
        ESP_LOGW(TAG, "MQTT not connected, event dropped");
        return ESP_ERR_INVALID_STATE;
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "yunxia/%s/event", device_id);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, json_payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published event to %s, msg_id=%d", topic, msg_id);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_response(const char *device_id, const char *json_payload)
{
    if (!s_mqtt_client || !s_mqtt_connected) {
        ESP_LOGW(TAG, "MQTT not connected, response dropped");
        return ESP_ERR_INVALID_STATE;
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "yunxia/%s/response", device_id);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, json_payload, 0, 1, 0);
    ESP_LOGI(TAG, "Published response to %s, msg_id=%d", topic, msg_id);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

esp_err_t mqtt_publish_status(const char *device_id, const char *status)
{
    if (!s_mqtt_client || !s_mqtt_connected) {
        return ESP_ERR_INVALID_STATE;
    }

    char topic[64];
    snprintf(topic, sizeof(topic), "yunxia/%s/status", device_id);

    int msg_id = esp_mqtt_client_publish(s_mqtt_client, topic, status, 0, 1, 0);
    ESP_LOGI(TAG, "Published status to %s: %s", topic, status);
    return (msg_id >= 0) ? ESP_OK : ESP_FAIL;
}

bool mqtt_is_connected(void)
{
    return s_mqtt_connected;
}

void mqtt_register_cmd_callback(mqtt_cmd_callback_t callback)
{
    s_cmd_callback = callback;
}
