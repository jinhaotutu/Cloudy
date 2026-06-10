// Adapt-Cloudy/driver/drv_st7789.h
// TFT ST7789 屏幕驱动（1.69寸，240×280，3-wire SPI）

#pragma once

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// RGB565 颜色定义
#define COLOR_BLACK       0x0000
#define COLOR_WHITE       0xFFFF
#define COLOR_RED         0xF800
#define COLOR_GREEN       0x07E0
#define COLOR_BLUE        0x001F
#define COLOR_YELLOW      0xFFE0
#define COLOR_CYAN        0x07FF
#define COLOR_MAGENTA     0xF81F
#define COLOR_GRAY        0x8410

/**
 * @brief 初始化 ST7789 屏幕
 *        包括 SPI 引脚初始化和屏幕寄存器配置
 */
void drv_st7789_init(void);

/**
 * @brief 设置绘图窗口
 * @param x0  起始列
 * @param y0  起始行
 * @param x1  结束列
 * @param y1  结束行
 */
void drv_st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief 填充整个屏幕为单一颜色
 * @param color  RGB565 颜色值
 */
void drv_st7789_fill_screen(uint16_t color);

/**
 * @brief 填充矩形区域
 * @param x      起始列
 * @param y      起始行
 * @param w      宽度
 * @param h      高度
 * @param color  RGB565 颜色值
 */
void drv_st7789_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief 绘制单个像素
 * @param x      列坐标
 * @param y      行坐标
 * @param color  RGB565 颜色值
 */
void drv_st7789_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 绘制 ASCII 字符（8×16 点阵）
 * @param x      起始列
 * @param y      起始行
 * @param ch     ASCII 字符
 * @param color  前景色
 * @param bg     背景色
 */
void drv_st7789_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg);

/**
 * @brief 绘制 ASCII 字符串
 * @param x      起始列
 * @param y      起始行
 * @param str    ASCII 字符串
 * @param color  前景色
 * @param bg     背景色
 */
void drv_st7789_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

/**
 * @brief 绘制缩放后的 ASCII 字符
 */
void drv_st7789_draw_char_scaled(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg, uint8_t scale);

/**
 * @brief 绘制缩放后的 ASCII 字符串
 */
void drv_st7789_draw_string_scaled(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);

/**
 * @brief 居中绘制缩放后的 ASCII 字符串（水平居中）
 * @param y      起始行
 * @param scale  放大倍数（1=原始大小，2=2倍，3=3倍）
 */
void drv_st7789_draw_string_center(uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);

// ============ 帧缓冲 API（PSRAM，页面合成后一次性刷新） ============

/**
 * @brief 初始化帧缓冲（在 PSRAM 中分配 240×280×2 字节）
 * @return true=成功, false=分配失败
 */
bool drv_st7789_fb_init(void);

/**
 * @brief 清除帧缓冲为指定颜色
 */
void drv_st7789_fb_clear(uint16_t color);

/**
 * @brief 将帧缓冲一次性写入屏幕
 */
void drv_st7789_fb_flush(void);

/**
 * @brief 在帧缓冲中绘制像素
 */
void drv_st7789_fb_draw_pixel(uint16_t x, uint16_t y, uint16_t color);

/**
 * @brief 在帧缓冲中填充矩形
 */
void drv_st7789_fb_fill_rect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t color);

/**
 * @brief 在帧缓冲中绘制字符
 */
void drv_st7789_fb_draw_char(uint16_t x, uint16_t y, char ch, uint16_t color, uint16_t bg);

/**
 * @brief 在帧缓冲中绘制字符串
 */
void drv_st7789_fb_draw_string(uint16_t x, uint16_t y, const char *str, uint16_t color, uint16_t bg);

/**
 * @brief 在帧缓冲中居中绘制缩放字符串
 */
void drv_st7789_fb_draw_string_center(uint16_t y, const char *str, uint16_t color, uint16_t bg, uint8_t scale);

/**
 * @brief 在帧缓冲中绘制 1.5 倍字符串（12×24）
 */
void drv_st7789_fb_draw_string_center_1_5x(uint16_t y, const char *str, uint16_t color, uint16_t bg);

#ifdef __cplusplus
}
#endif
