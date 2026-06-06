# Chip-Cloudy/Espressif/start-esp32s3/main/CMakeLists.txt
idf_component_register(
    SRCS "start.c" "st7789_debug.c"
    INCLUDE_DIRS "." "../../../../App-Cloudy/product"
    REQUIRES cloud_hal driver
)
