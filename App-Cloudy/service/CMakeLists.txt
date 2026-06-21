idf_component_register(
    SRCS "svc_storage.c" "svc_time.c" "wifi_mqtt.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_timer log esp-mqtt esp_wifi esp_netif esp_event cJSON
)
