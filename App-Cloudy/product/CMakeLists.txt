idf_component_register(
    SRCS "app_food.c" "app_ui.c" "app_alert.c"
    INCLUDE_DIRS "."
    REQUIRES driver cloud_hal service log
)
