// Adapt-Cloudy/hal/hal_spi.c
// SPI 硬件抽象层实现（4-wire bit-bang，CS 常低）
// MSB 先发，Mode 0

#include "hal_spi.h"
#include "hal_gpio.h"
#include "esp_log.h"

static const char *TAG = "hal_spi";

static int s_scl_gpio = -1;
static int s_sda_gpio = -1;
static int s_dc_gpio  = -1;

void hal_spi_init(int cs_gpio, int scl_gpio, int sda_gpio, int dc_gpio)
{
    s_scl_gpio = scl_gpio;
    s_sda_gpio = sda_gpio;
    s_dc_gpio  = dc_gpio;

    // CS 引脚：输出，常低（始终选中）
    if (cs_gpio >= 0) {
        hal_gpio_init(cs_gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE);
        hal_gpio_set_level(cs_gpio, false);
    }

    // SCL、SDA、DC 设为输出
    hal_gpio_init(s_scl_gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE);
    hal_gpio_init(s_sda_gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE);
    hal_gpio_init(s_dc_gpio, HAL_GPIO_DIR_OUTPUT, HAL_GPIO_PULL_NONE);

    // 初始状态：全部高电平
    hal_gpio_set_level(s_scl_gpio, true);
    hal_gpio_set_level(s_sda_gpio, true);
    hal_gpio_set_level(s_dc_gpio, true);

    ESP_LOGI(TAG, "SPI init: CS=GPIO%d(always low), SCL=GPIO%d, SDA=GPIO%d, DC=GPIO%d",
             cs_gpio, s_scl_gpio, s_sda_gpio, s_dc_gpio);
}

// 内部函数：发送单个字节（MSB 先发）
static void spi_write_byte(uint8_t data)
{
    for (int i = 7; i >= 0; i--) {
        hal_gpio_set_level(s_scl_gpio, false);
        hal_gpio_set_level(s_sda_gpio, (data >> i) & 0x01);
        hal_gpio_set_level(s_scl_gpio, true);
    }
}

void hal_spi_send_cmd(uint8_t cmd)
{
    hal_gpio_set_level(s_dc_gpio, false);  // DC=低 → 命令
    spi_write_byte(cmd);
}

void hal_spi_send_data(uint8_t data)
{
    hal_gpio_set_level(s_dc_gpio, true);   // DC=高 → 数据
    spi_write_byte(data);
}

void hal_spi_send_data_bulk(const uint8_t *data, size_t len)
{
    hal_gpio_set_level(s_dc_gpio, true);   // DC=高 → 数据
    for (size_t i = 0; i < len; i++) {
        spi_write_byte(data[i]);
    }
}

void hal_spi_send_data16(uint16_t data)
{
    hal_gpio_set_level(s_dc_gpio, true);   // DC=高 → 数据
    spi_write_byte((data >> 8) & 0xFF);    // 高字节先发
    spi_write_byte(data & 0xFF);           // 低字节后发
}
