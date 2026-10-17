/**
 * @file mpu6050.h
 * @author edu
 * @brief mpu6050 driver header file
 * @copyright Copyright (c) 2026
 * 
 */
#ifndef __MPU6050_H
#define __MPU6050_H

#include "stm32f1xx_hal.h"

/** @brief 采样率分频寄存器: 采样率 = 1000Hz / (1 + 写入值) */
#define MPU6050_REG_SMPLRT_DIV     0x19U

/** @brief 数字低通滤波器(DLPF)配置寄存器 */
#define MPU6050_REG_CONFIG         0x1AU

/** @brief 陀螺仪量程配置寄存器(0x00 = ±250°/s) */
#define MPU6050_REG_GYRO_CONFIG    0x1BU

/** @brief 加速度量程配置寄存器(0x00 = ±2g) */
#define MPU6050_REG_ACCEL_CONFIG   0x1CU

/** @brief 中断使能寄存器 */
#define MPU6050_REG_INT_ENABLE     0x38U

/** @brief 加速度 X 高字节: 14 字节连读的起点(0x3B~0x48) */
#define MPU6050_REG_ACCEL_XOUT_H   0x3BU

/** @brief 电源管理 1(复位 / 唤醒 / 时钟源) */
#define MPU6050_REG_PWR_MGMT_1     0x6BU

/** @brief 身份寄存器 */
#define MPU6050_REG_WHO_AM_I       0x75U


/** @brief 7 位从机地址: AD0 接 GND */
#define MPU6050_ADDR_AD0_LOW       0x68U

/** @brief 7 位从机地址: AD0 接 3.3V */
#define MPU6050_ADDR_AD0_HIGH      0x69U


/** @brief 加速度换算系数: 每 1g 对应 16384 个 LSB */
#define MPU6050_LSB_PER_G          16384.0f

/** @brief 陀螺仪换算系数: 每 1°/s 对应 131 个 LSB */
#define MPU6050_LSB_PER_DPS        131.0f

/**
  * @brief 一次读取的原始数据(全部是 16 位有符号数)
  */
typedef struct
{
  int16_t ax, ay, az;   /**< 加速度原始值, 除以 MPU6050_LSB_PER_G 得到单位 g */
  int16_t gx, gy, gz;   /**< 角速度原始值, 除以 MPU6050_LSB_PER_DPS 得到单位 °/s */
  int16_t temp;         /**< 温度原始值, 除以 340 再加 36.53 得到单位 °C */
} MPU6050_Raw_t;

/**
  * @brief  初始化 MPU6050
  * @param  hi2c: I2C 句柄
  * @retval 0=成功; 1=总线上找不到器件(检查接线/供电/AD0);
  *         2=找到了但配置写不进去
  */
uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c);

/**
  * @brief  一次读出 14 字节原始数据(加速度 XYZ  + 陀螺 XYZ)
  * @param  raw: 输出结构体指针
  * @retval 0=成功; 1=失败(I2C 通信错误)
  */
uint8_t MPU6050_ReadRaw(MPU6050_Raw_t *raw);

#endif /* __MPU6050_H */
