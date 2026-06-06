cmake_minimum_required(VERSION 3.22)

# 添加自定义组件路径
set(EXTRA_COMPONENT_DIRS
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../Adapt-Cloudy/cloud_hal"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../Adapt-Cloudy/driver"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../App-Cloudy/product"
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(cloud-start)
