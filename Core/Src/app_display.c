/**
  ******************************************************************************
  * @file    app_display.c
  * @brief   显示任务实现: 从队列取最新姿态 -> 一页数字界面画到 OLED

  ******************************************************************************
  */

#include "app_display.h"
#include "app_common.h"
#include "ssd1306.h"
#include "i2c.h"
#include "main.h"
#include <stdio.h>
#include <string.h>

#define DISP_PERIOD_MS   50U      /* 刷新周期: 50ms = 每秒画 20 帧 */
#define DISP_RETRY_MS    2000U    /* OLED 掉线后的重试间隔 */

/* 画一个 "°"(度) 符号: 8x8 字库里没有这个字符, 用 4 个像素拼个小圆圈 */
static void DrawDegree(uint8_t x, uint8_t y)
{
  SSD1306_DrawPixel((int16_t)x + 1, (int16_t)y,     1);
  SSD1306_DrawPixel((int16_t)x,     (int16_t)y + 1, 1);
  SSD1306_DrawPixel((int16_t)x + 2, (int16_t)y + 1, 1);
  SSD1306_DrawPixel((int16_t)x + 1, (int16_t)y + 2, 1);
}


#define LED_LEVEL_DEG   3.0f     /* 判"水平"的阈值(度) */

/**
  * @brief  根据当前姿态点亮对应的 LED
  * @param  m:    最新姿态消息
  * @param  have: 0 = 还没收到过消息
  * @retval 无
  */
static void Led_Update(const AttitudeMsg_t *m, uint8_t have)
{
  uint8_t green = 0U;
  uint8_t red   = 0U;
  uint8_t blue  = 0U;

  if ((have == 0U) || (m->status == APP_STATUS_CALIBRATING))
  {
    blue = 1U;                                                    /* 刚上电 / 校准中 */
  }
  else if (m->status == APP_STATUS_IMU_ERROR)
  {
    red = 1U;
  }
  else if ((m->roll  < LED_LEVEL_DEG) && (m->roll  > -LED_LEVEL_DEG) &&(m->pitch < LED_LEVEL_DEG) && (m->pitch > -LED_LEVEL_DEG))
  {
    green = 1U;                                                   /* 水平: 绿灯 */
  }
  else
  {
    blue = 1U;                                                    /* 倾斜: 蓝灯 */
  }

  HAL_GPIO_WritePin(LED_GREEN_GPIO_Port, LED_GREEN_Pin, (green != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_RED_GPIO_Port,   LED_RED_Pin,   (red   != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
  HAL_GPIO_WritePin(LED_BLUE_GPIO_Port,  LED_BLUE_Pin,  (blue  != 0U) ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

/**
  * @brief  把角度格式化成 "Roll: +12.34" 这样的字符串(两位小数)
  * @param  buf:   输出缓冲区(至少 24 字节)
  * @param  label: 前缀(如 "Roll: ")
  * @param  angle: 角度(度)
  * @retval 无
  */
static void FormatAngle(char *buf, const char *label, float angle)
{
  int32_t v;
  char    sign;

  if (angle < 0.0f) { sign = '-'; angle = -angle; }
  else              { sign = ' '; }


  v = (int32_t)(angle * 100.0f + 0.5f);
  sprintf(buf, "%s%c%d.%02d", label, sign, (int)(v / 100), (int)(v % 100));
}


static void DrawPage(const AttitudeMsg_t *m, uint8_t have)
{
  char line[24];

  SSD1306_Clear();
  SSD1306_DrawString(0, 0, "IMU Attitude");

  if (have == 0U)
  {
    SSD1306_DrawString(0, 24, "waiting...");
  }
  else if (m->status == APP_STATUS_IMU_ERROR)
  {
    SSD1306_DrawString(0, 24, "IMU ERROR!");
    SSD1306_DrawString(0, 40, "check wiring");
  }
  else if (m->status == APP_STATUS_CALIBRATING)
  {
    SSD1306_DrawString(0, 24, "Calibrating...");
    SSD1306_DrawString(0, 40, "keep it still");
  }
  else
  {

    FormatAngle(line, "Roll: ", m->roll);
    SSD1306_DrawString(0, 16, line);
    DrawDegree((uint8_t)(strlen(line) * 8U + 3U), 19U);

    FormatAngle(line, "Pitch:", m->pitch);
    SSD1306_DrawString(0, 32, line);
    DrawDegree((uint8_t)(strlen(line) * 8U + 3U), 35U);

    FormatAngle(line, "Yaw:  ", m->yaw);
    SSD1306_DrawString(0, 48, line);
    DrawDegree((uint8_t)(strlen(line) * 8U + 3U), 51U);
  }
}

/**
  * @brief  显示任务入口
  * @param  argument: 未使用
  * @retval 无
  */
void AppDisplay_Task(void *argument)
{
  osStatus_t    st;
  AttitudeMsg_t msg = {APP_STATUS_CALIBRATING, 0.0f, 0.0f, 0.0f};
  uint8_t       have     = 0U;
  uint8_t       oled_ok  = 0U;
  uint32_t      last_try = 0U;

  (void)argument;

  for (;;)
  {

    do
    {
      st = osMessageQueueGet(attitudeQueueHandle, &msg, NULL, 0U);
      if (st == osOK) { have = 1U; }
    } while (st == osOK);


    Led_Update(&msg, have);

    if (oled_ok == 0U)
    {
      if ((last_try == 0U) || ((HAL_GetTick() - last_try) >= DISP_RETRY_MS))
      {
        uint8_t ret;

        APP_I2C_LOCK();
        ret = SSD1306_Init(&hi2c2);
        APP_I2C_UNLOCK();

        last_try = HAL_GetTick();
        oled_ok  = (uint8_t)((ret == 0U) ? 1U : 0U);
      }
    }


    if (oled_ok != 0U)
    {
      uint8_t ret;

      APP_I2C_LOCK();
      DrawPage(&msg, have);
      ret = SSD1306_UpdateScreen();
      APP_I2C_UNLOCK();

      if (ret != 0U)
      {
        oled_ok = 0U;    
      }
    }

    osDelay(DISP_PERIOD_MS);
  }
}
