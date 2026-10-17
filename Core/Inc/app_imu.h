/**
  ******************************************************************************
  * @file    app_imu.h
  * @brief   IMU 任务(姿态解算)对外接口
  ******************************************************************************
  */
#ifndef __APP_IMU_H
#define __APP_IMU_H

#ifdef __cplusplus
extern "C" {
#endif

/**
  * @brief  IMU 任务入口
  * @param  argument: 未使用
  * @retval 无
  */
void AppImu_Task(void *argument);

#ifdef __cplusplus
}
#endif

#endif /* __APP_IMU_H */
