/**
  ******************************************************************************
  * @file    app_common.h
  * @brief   模块间公共定义: RTOS 对象句柄声明 + 消息类型 + I2C 锁宏
  ******************************************************************************
  */
#ifndef __APP_COMMON_H
#define __APP_COMMON_H

#include <stdint.h>
#include "cmsis_os2.h"



/** @brief I2C2 总线互斥锁(定义在 freertos.c) */
extern osMutexId_t i2cMutexHandle;

/** @brief 串口发送令牌(二值信号量, 初值 1; 定义在 freertos.c) */
extern osSemaphoreId_t uartTxDoneHandle;

/** @brief 姿态消息队列(深度 1 = 最新值优先; 定义在 freertos.c) */
extern osMessageQueueId_t attitudeQueueHandle;

/** @brief 串口接收帧队列(深度 4; 定义在 freertos.c) */
extern osMessageQueueId_t uartFrameQueueHandle;

/** @brief 串口单帧的最大字节数(同时也是 DMA 接收缓冲区大小) */
#define APP_UART_RX_BUF_SIZE   128U

/**
  * @brief 系统姿态状态(IMU 任务通过队列告诉显示任务"现在该显示哪一页")
  */
typedef enum
{
  APP_STATUS_CALIBRATING = 0,   /**< 正在校准陀螺零偏, 要求板子静止 */
  APP_STATUS_OK,                /**< 正常, 角度有效 */
  APP_STATUS_IMU_ERROR          /**< 找不到 IMU 或读失败 */
} AppStatus_t;

/**
  * @brief 一条姿态消息(单位: 度)
  */
typedef struct
{
  AppStatus_t status;   /**< 当前状态 */
  float       roll;     /**< 横滚角 */
  float       pitch;    /**< 俯仰角 */
  float       yaw;      /**< 偏航角(六轴方案下必然缓慢漂移) */
} AttitudeMsg_t;

/**
  * @brief 一帧串口数据
  */
typedef struct
{
  uint16_t len;                        /**< 本帧实际字节数(1~128) */
  uint8_t  data[APP_UART_RX_BUF_SIZE]; /**< 帧内容 */
} UartFrame_t;

/**
  * @brief 获取 I2C2 总线使用权(拿不到就一直等)
  */
#define APP_I2C_LOCK()                                          \
  do {                                                          \
    if (i2cMutexHandle != NULL) {                               \
      (void)osMutexAcquire(i2cMutexHandle, osWaitForever);      \
    }                                                           \
  } while (0)

/** @brief 释放 I2C2 总线(必须与 APP_I2C_LOCK() 成对出现) */
#define APP_I2C_UNLOCK()                                        \
  do {                                                          \
    if (i2cMutexHandle != NULL) {                               \
      (void)osMutexRelease(i2cMutexHandle);                     \
    }                                                           \
  } while (0)

#endif /* __APP_COMMON_H */