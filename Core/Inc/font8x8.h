/**
  ******************************************************************************
  * @file    font8x8.h
  * @brief   8x8 点阵 ASCII 字库(用于在 128x64 OLED 上显示英文和数字)
  * @note    来源: Daniel Hepper 的 font8x8 项目 (Public Domain / 公共领域)
  *          https://github.com/dhepper/font8x8
  *          数组下标 = 字符的 ASCII 码(0x20~0x7F);
  *          每个字符 8 个字节 = 从上到下 8 行, 字节的 bit0 是最左边的像素。
  ******************************************************************************
  */
#ifndef __FONT8X8_H
#define __FONT8X8_H


#include <stdint.h>

  /**
  * @brief  8x8 点阵字库数据(只读, 定义在 font8x8.c)
  * @note   数组下标 = 字符的 ASCII 码(0x20~0x7F);
  *         每个字符占 8 个字节 = 从上到下 8 行, 每个字节的 bit0 是最左边的像素。
  */
extern const uint8_t font8x8_basic[128][8];

#endif /* __FONT8X8_H */
