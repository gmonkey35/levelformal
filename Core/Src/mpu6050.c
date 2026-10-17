/**
 * @file mpu6050.c
 * @author edu
 * @brief MPU6050 driver implementation
 * 
 * @copyright Copyright (c) 2026
 * 
 */

#include "mpu6050.h"
#include "stm32f1xx_hal.h"

static I2C_HandleTypeDef *s_hi2c  = NULL;  
static uint8_t s_addr8 = 0U; 

/**
  * @brief  写一个寄存器
  * @param  reg: 寄存器地址
  * @param  val: 要写入的值
  * @retval 0=成功; 1=失败
  */
static uint8_t MPU6050_WriteReg(uint8_t reg, uint8_t val)
{
  if (HAL_I2C_Mem_Write(s_hi2c, s_addr8, reg, I2C_MEMADD_SIZE_8BIT, &val, 1U, 50U) != HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

/**
  * @brief  读一个寄存器
  * @param  reg: 寄存器地址
  * @param  val: 读出值存放处
  * @retval 0=成功; 1=失败
  */
static uint8_t MPU6050_ReadReg(uint8_t reg, uint8_t *val)
{
  if (HAL_I2C_Mem_Read(s_hi2c, s_addr8, reg, I2C_MEMADD_SIZE_8BIT, val, 1U, 50U) != HAL_OK)
  {
    return 1U;
  }
  return 0U;
}

/**
  * @brief  初始化 MPU6050
  * @param  hi2c: I2C 句柄
  * @retval 0=成功; 1=总线上找不到器件; 2=配置写不进去
  */
uint8_t MPU6050_Init(I2C_HandleTypeDef *hi2c)
{
  s_hi2c = hi2c;

  
  if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(MPU6050_ADDR_AD0_LOW  << 1), 3U, 50U) == HAL_OK)
  {
    s_addr8 = (uint8_t)(MPU6050_ADDR_AD0_LOW << 1);
  }
  else if (HAL_I2C_IsDeviceReady(hi2c, (uint16_t)(MPU6050_ADDR_AD0_HIGH << 1), 3U, 50U) == HAL_OK)
  {
    s_addr8 = (uint8_t)(MPU6050_ADDR_AD0_HIGH << 1);
  }
  else
  {
    return 1U;    
  }

  /* 唤醒(清 SLEEP 位) + 时钟选陀螺 X 轴 PLL */
  if (MPU6050_WriteReg(MPU6050_REG_PWR_MGMT_1, 0x01U) != 0U) { return 2U; }
  /* 采样率 = 1000 / (1 + 9) = 100Hz, 与任务 10ms 周期匹配 */
  if (MPU6050_WriteReg(MPU6050_REG_SMPLRT_DIV, 0x09U) != 0U) { return 2U; }
  /* 数字低通滤波(DLPF) = 3: 加速度 44Hz / 陀螺 42Hz(延迟约 4.9ms)。*/
  if (MPU6050_WriteReg(MPU6050_REG_CONFIG, 0x03U) != 0U) { return 2U; }
  /* 陀螺 ±250°/s (131 LSB per °/s) */
  if (MPU6050_WriteReg(MPU6050_REG_GYRO_CONFIG, 0x00U) != 0U) { return 2U; }
  /* 加速度 ±2g (16384 LSB per g) */
  if (MPU6050_WriteReg(MPU6050_REG_ACCEL_CONFIG, 0x00U) != 0U) { return 2U; }

  /* 配置回读验证*/
  {
    uint8_t pwr = 0U;

    if (MPU6050_ReadReg(MPU6050_REG_PWR_MGMT_1, &pwr) != 0U) { return 2U; }
    if (pwr != 0x01U) { return 2U; }
  }

  return 0U;
}

/**
  * @brief  一次读出 14 字节原始数据(加速度 XYZ  + 陀螺 XYZ)
  * @param  raw: 输出结构体
  * @retval 0=成功; 1=失败
  */
uint8_t MPU6050_ReadRaw(MPU6050_Raw_t *raw)
{
  uint8_t buf[14];

  if ((s_hi2c == NULL) || (raw == NULL))
  {
    return 1U;   /* 还没初始化过, 直接失败 */
  }

  /* 读取寄存器 */
  if (HAL_I2C_Mem_Read(s_hi2c, s_addr8, MPU6050_REG_ACCEL_XOUT_H, I2C_MEMADD_SIZE_8BIT, buf, 14U, 50U) != HAL_OK)
  {
    return 1U;
  }

  /* 拼成 16 位有符号数*/
  raw->ax   = (int16_t)(((uint16_t)buf[0]  << 8) | (uint16_t)buf[1]);
  raw->ay   = (int16_t)(((uint16_t)buf[2]  << 8) | (uint16_t)buf[3]);
  raw->az   = (int16_t)(((uint16_t)buf[4]  << 8) | (uint16_t)buf[5]);
  raw->temp = (int16_t)(((uint16_t)buf[6]  << 8) | (uint16_t)buf[7]);
  raw->gx   = (int16_t)(((uint16_t)buf[8]  << 8) | (uint16_t)buf[9]);
  raw->gy   = (int16_t)(((uint16_t)buf[10] << 8) | (uint16_t)buf[11]);
  raw->gz   = (int16_t)(((uint16_t)buf[12] << 8) | (uint16_t)buf[13]);

  return 0U;
}
