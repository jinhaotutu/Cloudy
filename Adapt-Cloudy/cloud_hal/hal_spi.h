// Adapt-Cloudy/hal/hal_spi.h
// SPI 硬件抽象层（4-wire bit-bang，带 CS 片选）
// 用于 TFT ST7789 屏幕通信

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 SPI 引脚（CS, SCL, SDA, DC）
 * @param cs_gpio   片选引脚（低电平有效），传 -1 表示不使用
 * @param scl_gpio  SPI 时钟引脚
 * @param sda_gpio  SPI 数据引脚（MOSI）
 * @param dc_gpio   数据/命令选择引脚
 */
void hal_spi_init(int cs_gpio, int scl_gpio, int sda_gpio, int dc_gpio);

/**
 * @brief 发送命令字节（DC=低电平）
 * @param cmd  命令字节
 */
void hal_spi_send_cmd(uint8_t cmd);

/**
 * @brief 发送数据字节（DC=高电平）
 * @param data  数据字节
 */
void hal_spi_send_data(uint8_t data);

/**
 * @brief 发送数据块（DC=高电平）
 * @param data  数据缓冲区
 * @param len   数据长度
 */
void hal_spi_send_data_bulk(const uint8_t *data, size_t len);

/**
 * @brief 发送 16 位数据（DC=高电平）
 * @param data  16 位数据
 */
void hal_spi_send_data16(uint16_t data);

#ifdef __cplusplus
}
#endif
