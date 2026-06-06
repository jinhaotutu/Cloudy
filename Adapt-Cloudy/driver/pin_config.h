// App-Cloudy/product/pin_config.h
// GPIO 引脚配置 — 基于 XIAO-ESP32S3-Sense
// 参考：C1_硬件选型.md, C2_硬件架构文档.md

#pragma once

// ============================================================
// 矩阵键盘 GPIO（3行 × 3列 = 6 GPIO）
// ============================================================
// XIAO D0 → GPIO1 — ROW0: K1(乳制品)、K2(肉蛋类)、K3(蔬菜类)
// XIAO D1 → GPIO2 — ROW1: K4(水果类)、K5(海鲜类)、K6(饮品类)
// XIAO D2 → GPIO3 — ROW2: K7(冷冻类)、(空位)、K8(确认)
// XIAO D3 → GPIO4 — COL0: K1、K4、K7
// XIAO D4 → GPIO5 — COL1: K2、K5、(空位)
// XIAO D5 → GPIO6 — COL2: K3、K6、K8

#define MATRIX_ROW0_GPIO    1   // XIAO D0
#define MATRIX_ROW1_GPIO    2   // XIAO D1
#define MATRIX_ROW2_GPIO    3   // XIAO D2
#define MATRIX_COL0_GPIO    4   // XIAO D3
#define MATRIX_COL1_GPIO    5   // XIAO D4
#define MATRIX_COL2_GPIO    6   // XIAO D5

#define MATRIX_ROWS         3
#define MATRIX_COLS         3

// ============================================================
// TFT 屏幕 GPIO（4-wire SPI）
// ============================================================
// XIAO D6  → GPIO43 — CS  (片选，低电平有效)
// XIAO D7  → GPIO44 — RST (硬件复位，低电平有效)
// XIAO D8  → GPIO7  — SCL (SPI 时钟)
// XIAO D9  → GPIO8  — DC  (数据/命令选择)
// XIAO D10 → GPIO9  — SDA (SPI 数据，即 MOSI)
// BL → VCC（背光常亮）

#define TFT_CS_GPIO         43  // XIAO D6 — Chip Select
#define TFT_RST_GPIO        44  // XIAO D7 — Reset
#define TFT_SCL_GPIO        7   // XIAO D8 — SPI CLK
#define TFT_DC_GPIO         8   // XIAO D9 — Data/Command
#define TFT_SDA_GPIO        9   // XIAO D10 — SPI MOSI

// TFT 屏幕参数
#define TFT_WIDTH           240
#define TFT_HEIGHT          280

// ============================================================
// LED GPIO
// ============================================================
// XIAO 板载用户 LED — GPIO21，高电平点亮

#define LED_GPIO            21  // 板载 LED

// ============================================================
// SPI 时序参数
// ============================================================

#define TFT_SPI_FREQ_HZ    40000000  // 40MHz
#define TFT_SPI_DELAY_NS   0         // 无额外延时
