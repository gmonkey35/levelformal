/**
  ******************************************************************************
  * @file    app_uart.c
  * @brief   串口回显任务实现: USART2 + DMA + 空闲中断, 变长数据帧无缝回显
  ******************************************************************************
  */

/* ------------------------------------------------------------------------------- */
#include "app_uart.h"
#include "app_common.h"
#include "usart.h"
#include "main.h"
#include <string.h>


extern DMA_HandleTypeDef hdma_usart2_rx;

/** @brief DMA 接收目的地 */
static uint8_t s_rx_buf[APP_UART_RX_BUF_SIZE];

/**
  * @brief  把接收"DMA + 空闲中断"挂起来, 准备接下一帧
  * @param  无
  * @retval 无
  */
static void AppUart_RxArm(void)
{
  if (HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_rx_buf, (uint16_t)sizeof(s_rx_buf)) == HAL_OK)
  {
    __HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT);
  }
}

/**
  * @brief  帧结束回调
  * @param  huart: HAL 传回的串口句柄
  * @param  Size:  本帧实际接收到的字节数
  * @retval 无
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
  UartFrame_t frame;



  if ((Size > 0U) && (Size <= APP_UART_RX_BUF_SIZE))
  {
    frame.len = Size;
    (void)memcpy(frame.data, s_rx_buf, Size);
    (void)osMessageQueuePut(uartFrameQueueHandle, &frame, 0U, 0U);
  }
  AppUart_RxArm();
}

/**
  * @brief  发送完成回调
  * @param  huart: HAL 传回的串口句柄
  * @retval 无
  */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    (void)osSemaphoreRelease(uartTxDoneHandle);
}



/**
  * @brief  串口回显任务入口
  * @param  argument: 未使用

  */
void AppUart_Task(void *argument)
{
  UartFrame_t frame;

  (void)argument;
  AppUart_RxArm();         

  for (;;)
  {
    if (osSemaphoreAcquire(uartTxDoneHandle, osWaitForever) != osOK) { continue; }

    if (osMessageQueueGet(uartFrameQueueHandle, &frame, NULL, 200U) == osOK)
    {
      if (HAL_UART_Transmit_DMA(&huart2, frame.data, frame.len) != HAL_OK)
      {
        (void)osSemaphoreRelease(uartTxDoneHandle); 
      }
    }
    else
    {
      (void)osSemaphoreRelease(uartTxDoneHandle);
    }
  }
}
