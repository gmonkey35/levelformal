# levelformal · STM32 电子水平仪

基于 **STM32F103C8T6 + MPU6050 + SSD1306 OLED** 的电子水平仪（倾角仪）固件。
FreeRTOS 多任务，上电自动校准陀螺零偏，用**互补滤波**解算 Roll / Pitch / Yaw，
0.96" OLED 实时显示角度，三色 LED 指示是否水平。

> 个人学习项目，代码带完整中文注释，可以当作「MPU6050 姿态解算入门」的工程范例来读。

---

## 功能

| 功能 | 说明 |
|---|---|
| 三轴姿态解算 | Roll / Pitch 用互补滤波（陀螺积分 + 加速度计重力修正），Yaw 用陀螺积分 |
| 上电零偏校准 | 静止采样 200 次 × 5ms 取平均，之后每拍减去零偏（`Imu_CalibrateGyro`） |
| 水平指示 | \|Roll\| 与 \|Pitch\| 都小于 3° 时绿灯亮，否则红灯亮（`Led_Update`） |
| 角度显示 | 128×64 OLED，`Roll: +12.35` 格式，自写定点格式化，不链浮点 printf |
| 串口调试 | USART2 115200 8N1，接收数据按原样回显 |
| 周期节拍 | IMU 任务 100Hz（10ms），显示任务 50ms，数据用深度 1 邮箱传递（最新值优先） |

## 硬件

| 器件 | 型号 / 参数 |
|---|---|
| MCU | STM32F103C8T6，Cortex-M3，64KB Flash / 20KB RAM，HSE ×9 = 72MHz |
| 姿态传感器 | MPU6050，I2C 接口，地址 0x68 / 0x69 自动探测 |
| 显示屏 | SSD1306 128×64 OLED，I2C 地址 0x3C |
| 指示灯 | LED 红 PA7、绿 PA6、蓝 PB0（高电平点亮） |
| 调试口 | USART2，115200 8N1 |

### 接线

| STM32 引脚 | 连接 |
|---|---|
| PB10 (I2C2_SCL) / PB11 (I2C2_SDA) | MPU6050 与 OLED 共用一条 I2C 总线，400kHz |
| PA2 (USART2_TX) / PA3 (USART2_RX) | USB-TTL 串口模块 |
| PA6 / PA7 / PB0 | 绿 / 红 / 蓝 LED |

> MPU6050 的 AD0 接 GND 为 0x68，接 VCC 为 0x69，代码两种都试。

## 目录结构

```
levelformal/
├── Core/
│   ├── Inc/          头文件（app_common.h 放任务间消息结构）
│   └── Src/
│       ├── main.c / freertos.c        初始化、任务创建
│       ├── app_imu.c                  IMU 任务：校准 + 互补滤波 + Yaw 积分
│       ├── app_display.c              显示任务：OLED 排版 + LED 控制
│       ├── app_uart.c                 串口任务：DMA 收发 + 回显
│       ├── mpu6050.c / ssd1306.c      传感器与屏幕驱动
│       └── stm32f1xx_*.c              HAL 初始化（CubeMX 生成）
├── Drivers/           STM32F1xx HAL + CMSIS（CubeMX 生成）
├── Middlewares/       FreeRTOS Kernel
├── MDK-ARM/           Keil MDK 工程（levelformal.uvprojx）
│   └── .eide/eide.yml VSCode + EIDE 插件工程配置
└── levelformal.ioc    STM32CubeMX 工程文件
```

## 编译 / 下载

**Keil MDK5**（推荐，仓库自带工程）
1. 打开 `MDK-ARM/levelformal.uvprojx`
2. F7 编译，F8 下载（需 ARM Compiler 5；STM32F1xx 器件包在 Keil 里装好）
3. 若下载后不运行，检查 `Debug → Settings → Flash Download` 是否勾选 Reset and Run

**VSCode + EIDE 插件**
1. 打开 `MDK-ARM/` 目录，EIDE 会读取 `.eide/eide.yml`
2. 选好 ARMCC/AC5 工具链后编译、烧录

**STM32CubeMX**：`levelformal.ioc` 可重新生成 `Core/Src/stm32f1xx_*.c` 等 HAL 初始化代码（不要覆盖 `app_*.c`）。

## 算法要点

1. **零偏校准**：静止时陀螺输出 = 零偏 + 噪声。采 200 次求平均，噪声被压到 1/√200 ≈ 1/14，得到零偏后每拍减去。
2. **互补滤波**（`α = 0.98`，等效时间常数 τ = dt/(1-α) ≈ 0.5s）：
   `angle = α·(angle + gyro·dt) + (1-α)·accel_angle`
   陀螺动态好但有漂移，加速度计有绝对重力基准但噪声大，两者互补，Roll / Pitch 不会长期漂。
3. **Yaw**：没有绝对参考（无磁力计），只能纯积分 `yaw += gz·dt`，所以会随时间缓慢漂移；代码里的 ±180° 绕圈只是防止数值一直累加。

### 已知限制

- **Yaw 会漂**：MPU6050 零偏随温度变化（规格书全温区 ±20°/s），没有磁力计就无法长期锁住航向。
- **未做温度补偿**：`MPU6050_ReadRaw` 已经把片上温度读进 `raw.temp`，但校准与解算都没用它。
- **运行中 I2C 掉线不报警**：只有上电初始化失败才会显示 `IMU ERROR!`；运行中读失败只是丢掉这一拍，屏幕会定格在最后一个角度。
- **无静止检测**：没有 ZUPT（零速修正），板子静止时零偏残余仍会被积分。

## 许可证

本项目自写代码采用 [MIT](LICENSE)。第三方组件版权归各自作者：

| 组件 | 许可证 |
|---|---|
| STM32F1xx HAL Driver / CMSIS Device | BSD-3-Clause（STMicroelectronics） |
| CMSIS Core | Apache-2.0（ARM Limited） |
| FreeRTOS Kernel | MIT（Amazon / Richard Barry） |
