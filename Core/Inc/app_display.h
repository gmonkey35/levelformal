/**
  ******************************************************************************
  * @file    app_display.h
  * @brief   显示任务(OLED)对外接口
  ******************************************************************************
  */

#ifndef __APP_DISPLAY_H
#define __APP_DISPLAY_H

#include "app_common.h"


#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  显示任务入口(osThreadNew 的回调)
  * @param  argument: 未使用
  * @retval 无(任务函数永不返回)
  */
void AppDisplay_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __APP_DISPLAY_H */    

