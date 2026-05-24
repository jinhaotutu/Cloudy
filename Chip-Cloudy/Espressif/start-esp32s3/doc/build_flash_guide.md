# ESP32 项目编译烧录验证指南

## 1. 项目概述

本指南详细描述了 Cloudy 项目的编译、烧录和验证流程。

> **路径说明**: 本文档中所有路径均为相对路径，根目录为 `Cloudy/`。

- **项目路径**: `Chip-Cloudy/Espressif/start-esp32s3/`
- **ESP-IDF 路径**: `Chip-Cloudy/Espressif/esp-idf/`
- **目标芯片**: ESP32-S3

## 2. 编译流程

### 2.1 环境准备

在编译前需要激活 ESP-IDF 环境：

```bash
cd Chip-Cloudy/Espressif/esp-idf
source export.sh
```

### 2.2 进入项目目录

```bash
cd Chip-Cloudy/Espressif/start-esp32s3
```

### 2.3 执行编译

```bash
idf.py build
```

### 2.4 编译成功输出示例

```
[100%] Built target __idf_main
[100%] Built target __ldgen_output_sections.ld
[100%] Built target cloud-start.elf
[100%] Built target gen_cloud-start_binary
[100%] Built target gen_project_binary
cloud-start.bin binary size 0x2ab10 bytes. Smallest app partition is 0x100000 bytes. 0xd54f0 bytes (83%) free.
[100%] Built target app_check_size
[100%] Built target app

Project build complete. To flash, run:
 idf.py flash
or
 idf.py -p PORT flash
```

## 3. 烧录流程

### 3.1 确认串口设备

首先检查系统中可用的串口设备：

```bash
ls -la /dev/tty.* /dev/cu.* | grep -i usb
```

输出示例：
```
crw-rw-rw-  1 root  wheel  0x9000003  5 24 14:03 /dev/cu.usbmodem21201
crw-rw-rw-  1 root  wheel  0x9000002  5 23 18:47 /dev/tty.usbmodem21201
```

### 3.2 执行烧录

使用 `idf.py` 进行烧录：

```bash
idf.py -p /dev/tty.usbmodem21201 flash
```

### 3.3 烧录成功输出示例

```
esptool v5.3.dev3
Connected to ESP32-S3 on /dev/tty.usbmodem21201:
Chip type:          ESP32-S3 (QFN56) (revision v0.2)
Features:           Wi-Fi, BT 5 (LE), Dual Core + LP Core, 240MHz, Embedded PSRAM 8MB (AP_3v3)
Crystal frequency:  40MHz
USB mode:           USB-Serial/JTAG
MAC:                1c:db:d4:5b:7e:8c

Writing 'bootloader/bootloader.bin' at 0x00000000...
Wrote 21056 bytes (13534 compressed) at 0x00000000 in 0.3 seconds (614.3 kbit/s).
Hash of data verified.

Writing 'partition_table/partition-table.bin' at 0x00008000...
Wrote 3072 bytes (103 compressed) at 0x00008000 in 0.0 seconds (773.5 kbit/s).
Hash of data verified.

Writing 'cloud-start.bin' at 0x00010000...
Wrote 174864 bytes (96770 compressed) at 0x00010000 in 1.3 seconds (1045.9 kbit/s).
Hash of data verified.

Hard resetting via RTS pin...
[100%] Built target flash
Done
```

## 4. 烧录成功验证

### 4.1 启动串口监控

```bash
idf.py -p /dev/tty.usbmodem21201 monitor
```

### 4.2 烧录成功判断依据

设备启动后，串口日志输出以下关键信息表示烧录成功：

```
I (24) boot: ESP-IDF v6.0.1 2nd stage bootloader
I (24) boot: compile time May 24 2026 16:45:48
I (25) boot: Multicore bootloader
I (25) boot: chip revision: v0.2
```

**验证要点**:
- `ESP-IDF v6.0.1 2nd stage bootloader` - Bootloader 正确加载
- `compile time` - 显示编译时间戳
- `Multicore bootloader` - 多核启动模式正常
- `chip revision: v0.2` - 芯片版本识别正确

### 4.3 应用程序运行日志

应用程序成功运行后，会输出以下日志：

```
I (745) cloud-start: Cloud Start Minimal ESP32 Project
I (755) cloud-start: This is esp32s3 chip with 2 CPU core(s), WiFi/BLE
I (765) cloud-start: Silicon revision v0.2
I (765) cloud-start: 8MB external flash
I (765) cloud-start: Minimum free heap size: 8745164 bytes
I (785) cloud-start: Running...
```

### 4.4 退出监控

按 `Ctrl+C` 退出串口监控模式。