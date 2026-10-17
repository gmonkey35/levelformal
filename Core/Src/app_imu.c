/**
  ******************************************************************************
  * @file    app_imu.c
  * @brief   IMU 任务实现: 读 MPU6050 -> 互补滤波解算 -> 广播姿态消息
  ******************************************************************************
  */



#include "app_imu.h"
#include "app_common.h"   
#include "mpu6050.h"
#include "i2c.h"         
#include "main.h"         /* HAL_GetTick / osDelay */
#include <math.h>         /* atan2f / sqrtf */

#define IMU_PERIOD_MS        10U      /* 主循环周期: 10ms = 每秒解算 100 次 */
#define IMU_CALIB_SAMPLES    200U     /* 校准采样次数 */
#define IMU_CALIB_INTERVAL_MS 5U      /* 校准采样间隔, 200 × 5ms ≈ 1 秒 */
#define IMU_FILTER_ALPHA     0.98f    /* 互补滤波系数: 陀螺 98% + 加速度计 2% */
#define IMU_RAD_TO_DEG       57.29578f


static float s_roll,  s_pitch, s_yaw;         /* 解算出的三个角度(度) */
static float s_bias_gx, s_bias_gy, s_bias_gz; /* 陀螺仪三轴零偏(°/s), 校准得到 */

/**
  * @brief  把一条姿态消息放进队列(最新值优先策略)
  * @param  status: 状态; r/p/y: 三个角度(度)
  * @retval 无
  */
static void Imu_Publish(AppStatus_t status, float r, float p, float y)
{
  AttitudeMsg_t msg;

  msg.status = status;
  msg.roll   = r;
  msg.pitch  = p;
  msg.yaw    = y;

  if (osMessageQueuePut(attitudeQueueHandle, &msg, 0U, 0U) != osOK)
  {
    AttitudeMsg_t old;
    (void)osMessageQueueGet(attitudeQueueHandle, &old, NULL, 0U);  /* 丢旧 */
    (void)osMessageQueuePut(attitudeQueueHandle, &msg, 0U, 0U);    /* 放新 */
  }
}

/**
  * @brief  陀螺零偏校准: 静止时陀螺理论输出 0, 实际有个恒定小偏置, 采 200 次取平均
  * @param  无
  * @retval 无
  */
static void Imu_CalibrateGyro(void)
{
  float sx = 0.0f, sy = 0.0f, sz = 0.0f;
  MPU6050_Raw_t r;
  uint8_t  ok;
  uint16_t i, n = 0U;

  for (i = 0U; i < IMU_CALIB_SAMPLES; i++)
  {
    APP_I2C_LOCK();
    ok = (uint8_t)(MPU6050_ReadRaw(&r) == 0U);
    APP_I2C_UNLOCK();

    if (ok != 0U)
    {
      sx += (float)r.gx;
      sy += (float)r.gy;
      sz += (float)r.gz;
      n++;
    }
    osDelay(IMU_CALIB_INTERVAL_MS);
  }

  if (n > 0U)
  {
    s_bias_gx = (sx / (float)n) / MPU6050_LSB_PER_DPS;
    s_bias_gy = (sy / (float)n) / MPU6050_LSB_PER_DPS;
    s_bias_gz = (sz / (float)n) / MPU6050_LSB_PER_DPS;
  }
}

/**
  * @brief  用加速度计给 Roll/Pitch 一个合理起点: 静止时重力方向就能反推倾斜角
  * @param  无
  * @retval 无
  */
static void Imu_SeedAnglesFromAccel(void)
{
  MPU6050_Raw_t raw;
  float ax, ay, az;

  APP_I2C_LOCK();
  if (MPU6050_ReadRaw(&raw) == 0U)
  {
    APP_I2C_UNLOCK();

    ax = (float)raw.ax / MPU6050_LSB_PER_G;
    ay = (float)raw.ay / MPU6050_LSB_PER_G;
    az = (float)raw.az / MPU6050_LSB_PER_G;

    s_roll  = atan2f(ay, az) * IMU_RAD_TO_DEG;
    s_pitch = atan2f(-ax, sqrtf(ay * ay + az * az)) * IMU_RAD_TO_DEG;
    s_yaw   = 0.0f;
  }
  else
  {
    APP_I2C_UNLOCK();
    s_roll = 0.0f; s_pitch = 0.0f; s_yaw = 0.0f;
  }
}

/**
  * @brief  IMU 任务入口
  * @param  argument: 未使用(osThreadNew 传进来的参数)
  * @retval 无
  * @note   生命周期: 初始化(失败每 2 秒重试) -> 校准(约 1 秒) -> 定时解算循环。
  */
void AppImu_Task(void *argument)
{
  MPU6050_Raw_t raw;
  uint8_t  ret;
  uint32_t t_start, now, last_tick, elapsed;
  float    dt, ax, ay, az, gx, gy, gz, roll_acc, pitch_acc;

  (void)argument;

  for (;;)
  {
    APP_I2C_LOCK();
    ret = MPU6050_Init(&hi2c2);
    APP_I2C_UNLOCK();

    if (ret == 0U) { break; }

    Imu_Publish(APP_STATUS_IMU_ERROR, 0.0f, 0.0f, 0.0f);
    osDelay(2000U);
  }

  /*第 2 步: 校准*/
  Imu_Publish(APP_STATUS_CALIBRATING, 0.0f, 0.0f, 0.0f);
  Imu_CalibrateGyro();
  Imu_SeedAnglesFromAccel();   /* 用重力方向给角度一个合理起点 */


  last_tick = HAL_GetTick();
  for (;;)
  {
    t_start = HAL_GetTick();

    APP_I2C_LOCK();
    ret = MPU6050_ReadRaw(&raw);
    APP_I2C_UNLOCK();

    if (ret != 0U)
    {
      osDelay(IMU_PERIOD_MS);
      continue;
    }

  
    now       = HAL_GetTick();
    dt        = (float)(now - last_tick) * 0.001f;
    last_tick = now;
    if (dt < 0.001f) { dt = 0.001f; }
    if (dt > 0.100f) { dt = 0.100f; }

    ax = (float)raw.ax / MPU6050_LSB_PER_G;
    ay = (float)raw.ay / MPU6050_LSB_PER_G;
    az = (float)raw.az / MPU6050_LSB_PER_G;
    gx = (float)raw.gx / MPU6050_LSB_PER_DPS - s_bias_gx;
    gy = (float)raw.gy / MPU6050_LSB_PER_DPS - s_bias_gy;
    gz = (float)raw.gz / MPU6050_LSB_PER_DPS - s_bias_gz;


    roll_acc  = atan2f(ay, az) * IMU_RAD_TO_DEG;
    pitch_acc = atan2f(-ax, sqrtf(ay * ay + az * az)) * IMU_RAD_TO_DEG;

    /* 互补滤波*/
    s_roll  = IMU_FILTER_ALPHA * (s_roll  + gx * dt) + (1.0f - IMU_FILTER_ALPHA) * roll_acc;
    s_pitch = IMU_FILTER_ALPHA * (s_pitch + gy * dt) + (1.0f - IMU_FILTER_ALPHA) * pitch_acc;
    s_yaw  += gz * dt;


    if (s_yaw > 180.0f)       { s_yaw -= 360.0f; }
    else if (s_yaw < -180.0f) { s_yaw += 360.0f; }

    Imu_Publish(APP_STATUS_OK, s_roll, s_pitch, s_yaw);

    /* 补齐周期, 让循环尽量 10ms 一拍 */
    elapsed = HAL_GetTick() - t_start;
    if (elapsed < IMU_PERIOD_MS)
    {
      osDelay(IMU_PERIOD_MS - elapsed);
    }
  }
}