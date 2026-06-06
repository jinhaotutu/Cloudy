# Adapt-Cloudy/driver/CMakeLists.txt
idf_component_register(
    SRCS "drv_matrix_key.c" "drv_st7789.c" "drv_led.c"
    INCLUDE_DIRS "."
    REQUIRES cloud_hal freertos main
)
