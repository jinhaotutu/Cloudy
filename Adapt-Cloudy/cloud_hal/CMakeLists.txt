# Adapt-Cloudy/cloud_hal/CMakeLists.txt
idf_component_register(
    SRCS "hal_gpio.c" "hal_spi.c" "hal_timer.c"
    INCLUDE_DIRS "."
    REQUIRES esp_timer esp_driver_gpio
)
