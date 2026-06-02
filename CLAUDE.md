# CLAUDE.md — Cloudy 项目指南

## 项目概述

冰箱过期提醒器（Cloudy）嵌入式固件项目，基于 ESP32-S3 平台，使用 ESP-IDF 框架。

- **原型阶段**：XIAO-ESP32S3-Sense + 3×3 矩阵键盘 + ST7789 TFT 屏幕
- **正式阶段**：ESP32-S3-WROOM-1-N8R8 模组 + OLED 屏幕

## 目录结构

```
Cloudy/
├── esp32s3_start/          # 主程序入口（app_main）
│   ├── main/
│   │   ├── CMakeLists.txt
│   │   └── main.c
│   ├── CMakeLists.txt
│   ├── sdkconfig
│   └── partitions.csv
├── adapt/                  # 适配层
│   ├── hal/                # 硬件抽象层（GPIO/SPI/Timer）
│   └── driver/             # 驱动层（矩阵键盘/ST7789/LED）
├── app/
│   ├── product/            # 产品应用（食材管理/UI/告警）
│   └── service/            # 服务层（NVS存储/时间/BLE配网）
└── Chip-Cloudy/
    └── Espressif/
        └── esp-idf/        # ESP-IDF 框架（git submodule）
```

## 分层架构

```
应用层 (app/product/)  →  服务层 (app/service/)  →  驱动层 (adapt/driver/)  →  HAL层 (adapt/hal/)  →  ESP-IDF SDK
```

- 上层可调用下层，禁止反向依赖
- 同层模块间通过队列/信号量通信，不直接调用

## 构建

```bash
cd esp32s3_start
idf.py build
idf.py flash monitor
```

## 代码规范

- 模块命名：`app_`（产品应用）、`svc_`（服务）、`drv_`（驱动）、`hal_`（硬件抽象）
- 头文件用 `#pragma once`
- 函数命名：`模块_动作`，如 `app_food_add()`、`drv_matrix_scan()`

## 关键设计决策

- 多线程架构：主线程（业务逻辑）+ 按键扫描线程 + 定时器线程
- 线程间通信：FreeRTOS 队列（按键事件）+ 信号量（过期检查）
- 存储：NVS 键值对，单条食材记录 12 字节
- 屏幕：3-wire SPI，行缓冲渲染（无全帧缓冲）

## 参考文档

- 需求：`02_产品/冰箱过期提醒器/Q1_功能需求文档.md`
- 交互规格：`02_产品/冰箱过期提醒器/Q2_设备端交互规格.md`
- 硬件选型：`02_产品/冰箱过期提醒器/C1_硬件选型.md`
- 硬件架构：`02_产品/冰箱过期提醒器/C2_硬件架构文档.md`
