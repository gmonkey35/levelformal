/**
  ******************************************************************************
  * @file    ssd1306.c
  * @brief   SSD1306 128x64 OLED 驱动(I2C 接口, 带 1KB 显存)
  ******************************************************************************
  */

#include "ssd1306.h"
#include "font8x8.h"
#include <string.h>

static I2C_HandleTypeDef *s_hi2c = NULL;                       /* I2C 句柄 */
static uint8_t s_buffer[SSD1306_WIDTH * SSD1306_HEIGHT / 8];   /* 1024 字节显存 */



/**
  * @brief  发送一组指令
  * @param  cmds: 指令数组(每条指令一个字节)
  * @param  len:  指令条数
  * @retval 0=成功, 1=失败
  */
static uint8_t SSD1306_WriteCmd(const uint8_t *cmds, uint16_t len)
{
  if (HAL_I2C_Mem_Write(s_hi2c, SSD1306_ADDR_8BIT, 0x00U, I2C_MEMADD_SIZE_8BIT,(uint8_t *)cmds, len, 50) != HAL_OK)
  {
    return 1;
  }
  return 0;
}


/**
  * @brief  发送一组显存数据
  * @param  data: 数据指针
  * @param  len:  数据长度
  * @retval 0=成功, 1=失败
  */
static uint8_t SSD1306_WriteData(const uint8_t *data, uint16_t len)
{
  if (HAL_I2C_Mem_Write(s_hi2c, SSD1306_ADDR_8BIT, 0x40U, I2C_MEMADD_SIZE_8BIT,(uint8_t *)data, len, 100) != HAL_OK)
  {
    return 1;
  }
  return 0;
}


/**
  * @brief  初始化 OLED
  * @param  hi2c: I2C 句柄(本工程传入 &hi2c2)
  * @retval 0=成功, 1=失败
  */
uint8_t SSD1306_Init(I2C_HandleTypeDef *hi2c)
{
  static const uint8_t init_seq[] = {
    0xAE,        /* 关显示 */
    0xD5, 0x80,  /* 显示时钟分频 */
    0xA8, 0x3F,  /* 多路复用 = 64 行 */
    0xD3, 0x00,  /* 显示偏移 = 0 */
    0x40,        /* 显示起始行 = 0 */
    0x8D, 0x14,  /* 电荷泵打开(模块自带升压, 必须开) */
    0x20, 0x02,  /* 页寻址模式(本驱动按页刷新) */
    0xA1,        /* 水平方向不镜像 */
    0xC8,        /* 垂直方向不镜像 */
    0xDA, 0x12,  /* COM 引脚硬件配置 */
    0x81, 0xCF,  /* 对比度(0xCF 偏亮, 0x7F 偏暗) */
    0xD9, 0xF1,  /* 预充电周期 */
    0xDB, 0x40,  /* VCOMH 电压 */
    0xA4,        /* 显示内容跟随显存 */
    0xA6,        /* 正常显示(不反色) */
    0x2E,        /* 关闭滚动 */
    0xAF         /* 开显示 */
  };

  s_hi2c = hi2c;

  /* 确认屏幕在线 */
  if (HAL_I2C_IsDeviceReady(s_hi2c, SSD1306_ADDR_8BIT, 3, 50) != HAL_OK)
  {
    return 1;
  }
  if (SSD1306_WriteCmd(init_seq, sizeof(init_seq)) != 0)
  {
    return 1;
  }

  SSD1306_Clear();
  return SSD1306_UpdateScreen();
}

/**
  * @brief  把显存里的一整帧刷到屏幕(8 页 x 128 字节)
  * @retval 0=成功, 1=失败
  */
uint8_t SSD1306_UpdateScreen(void)
{
  uint8_t page;
  uint8_t cmd[3];

  for (page = 0; page < (SSD1306_HEIGHT / 8U); page++)
  {
    cmd[0] = (uint8_t)(0xB0U | page);   /* 定位到第 page 页 */
    cmd[1] = 0x00U;                     /* 列地址低 4 位 = 0 */
    cmd[2] = 0x10U;                     /* 列地址高 4 位 = 0 */
    if (SSD1306_WriteCmd(cmd, 3) != 0)
    {
      return 1;
    }
    if (SSD1306_WriteData(&s_buffer[(uint16_t)page * SSD1306_WIDTH], SSD1306_WIDTH) != 0)
    {
      return 1;
    }
  }
  return 0;
}


/**
  * @brief  清空显存(全黑)
  */
void SSD1306_Clear(void)
{
  memset(s_buffer, 0, sizeof(s_buffer));
}



/**
  * @brief  画一个像素
  * @param  x, y: 坐标(屏幕左上角是原点)
  * @param  on: 1=点亮, 0=熄灭
  */
void SSD1306_DrawPixel(int16_t x, int16_t y, uint8_t on)
{
  if (x < 0 || x >= SSD1306_WIDTH || y < 0 || y >= SSD1306_HEIGHT)
  {
    return;
  }

  if (on)
  {
    s_buffer[(uint16_t)x + (uint16_t)(y / 8) * SSD1306_WIDTH] |= (uint8_t)(1U << (y % 8));
  }
  else
  {
    s_buffer[(uint16_t)x + (uint16_t)(y / 8) * SSD1306_WIDTH] &= (uint8_t)~(1U << (y % 8));
  }
}


/**
  * @brief  画一个 8x8 字符(仅支持可见 ASCII)
  * @param  x,y: 字符左上角坐标; ch: 要画的字符
  */
void SSD1306_DrawChar(uint8_t x, uint8_t y, char ch)
{
  uint8_t i, j, b;

  if (ch < 0x20 || ch > 0x7F)
  {
    ch = '?';
  }

  for (i = 0; i < 8; i++)
  {
    b = font8x8_basic[(uint8_t)ch][i];   /* 第 i 行: 从上到下 */
    for (j = 0; j < 8; j++)
    {
      if (b & (uint8_t)(1U << j))        /* bit0 是最左边的像素 */
      {
        SSD1306_DrawPixel((int16_t)x + j, (int16_t)y + i, 1);
      }
    }
  }
}


/**
  * @brief  画字符串(每个字符宽 8 像素)
  * @param  x,y: 起始坐标; str: 字符串
  */
void SSD1306_DrawString(uint8_t x, uint8_t y, const char *str)
{
  while (*str != '\0')
  {
    SSD1306_DrawChar(x, y, *str);
    str++;
    x = (uint8_t)(x + 8);
  }
}