/**
  ******************************************************************************
  * @file    app_uart.h
  * @brief   串口回显任务对外接口
  ******************************************************************************
  */
#ifndef __APP_UART_H
#define __APP_UART_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  串口回显任务入口
  * @param  argument: 未使用
  * @retval 无
  */
void AppUart_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __APP_UART_H */
