# Chip-Cloudy/Espressif/start-esp32s3/main/CMakeLists.txt
idf_component_register(
    SRCS "start.c"
    INCLUDE_DIRS "."
    REQUIRES product
)
