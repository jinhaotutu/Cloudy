idf_component_register(
    SRCS "svc_storage.c" "svc_time.c"
    INCLUDE_DIRS "."
    REQUIRES nvs_flash esp_timer log
)
