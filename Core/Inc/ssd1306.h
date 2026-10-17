/**
  ******************************************************************************
  * @file    ssd1306.h
  * @brief   SSD1306 128x64 OLED 驱动(I2C 接口)
  ******************************************************************************
  */
#ifndef __SSD1306_H
#define __SSD1306_H

#include "stm32f1xx_hal.h"

/** @brief 屏幕宽度(像素) */
#define SSD1306_WIDTH    128U

/** @brief 屏幕高度(像素) */
#define SSD1306_HEIGHT   64U

/** @brief OLED 的 7 位 I2C 地址 */
#define SSD1306_ADDR_7BIT   0x3CU

/** @brief OLED 的 8 位 I2C 地址 */
#define SSD1306_ADDR_8BIT   (SSD1306_ADDR_7BIT << 1)

/**
  * @brief  初始化 OLED
  * @param  hi2c: I2C 句柄
  * @retval 0=成功; 1=失败
  */
uint8_t SSD1306_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief  把显存里的一整帧刷到屏幕(8 页 x 128 字节)
  * @retval 0=成功; 1=失败
  */
uint8_t SSD1306_UpdateScreen(void);

/**
  * @brief  清空显存
  */
void SSD1306_Clear(void);

/**
  * @brief  点亮/熄灭一个像素
  * @param  x:  横坐标(0~127)
  * @param  y:  纵坐标(0~63)
  * @param  on: 1=点亮, 0=熄灭
  */
void SSD1306_DrawPixel(int16_t x, int16_t y, uint8_t on);

/**
  * @brief  画一个 8x8 字符
  * @param  x:  字符左上角横坐标
  * @param  y:  字符左上角纵坐标
  * @param  ch: 要显示的字符(仅支持可见 ASCII, 0x20~0x7E)
  */
void SSD1306_DrawChar(uint8_t x, uint8_t y, char ch);

/**
  * @brief  画一串字符
  * @param  x:   起始横坐标
  * @param  y:   起始纵坐标
  * @param  str: 以 '\0' 结尾的字符串
  * @retval 无
  * @note   超出屏幕右边界或下边界的部分会被自动截断
  */
void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str);

#endif /* __SSD1306_H */
