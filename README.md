# levelformal · STM32 电子水平仪

基于 **STM32F103C8T6 + MPU6050 + SSD1306 OLED** 的电子水平仪（倾角仪）固件：FreeRTOS 多任务，
上电自动校准陀螺零偏，用**互补滤波**解算 Roll / Pitch / Yaw，0.96" OLED 实时显示角度，
三色 LED 指示是否水平，USART2 + DMA 实现不定长数据帧的原样回显。

> 个人学习项目，代码带完整中文 Doxygen 注释，可以当作「MPU6050 姿态解算 + HAL DMA 收发」的工程范例来读。

---

## 功能

| 功能 | 说明 |
|---|---|
| 三轴姿态解算 | Roll / Pitch 互补滤波（陀螺积分 + 加速度计重力修正），Yaw 陀螺积分 |
| 上电零偏校准 | 静止采样 200 次 × 5ms 取平均，之后每拍减去零偏（`Imu_CalibrateGyro`） |
| 水平指示 | \|Roll\| 与 \|Pitch\| 都小于 3° 时绿灯亮，否则红灯亮（`Led_Update`） |
| 角度显示 | 128×64 OLED，`Roll:  12.35` 格式，自写定点格式化，不链浮点 printf |
| 串口回显 | USART2 115200 + DMA，收到一帧完整数据后逐字节原样发回 |
| 周期节拍 | IMU 任务 100Hz（10ms），显示任务 50ms，数据用深度 1 邮箱传递（最新值优先） |

## 硬件

| 器件 | 型号 / 参数 |
|---|---|
| MCU | STM32F103C8T6，Cortex-M3，64KB Flash / 20KB RAM，HSE 8MHz ×9 = 72MHz |
| 姿态传感器 | MPU6050 六轴模块（裸传感器，无内置解算），I2C，地址 0x68 / 0x69 自动探测 |
| 显示屏 | SSD1306 128×64 OLED（I2C 接口，0x3C），**非串口屏** |
| 指示灯 | LED 红 PA7、绿 PA6、蓝 PB0（高电平点亮） |
| 通信 | USB-TTL 模块接 USART2，115200 8N1 |

### 接线

| STM32 引脚 | 连接 |
|---|---|
| PB10 (I2C2_SCL) / PB11 (I2C2_SDA) | MPU6050 与 OLED 共用一条 I2C 总线，400kHz，需接 4.7kΩ 上拉 |
| PA2 (USART2_TX) / PA3 (USART2_RX) | USB-TTL 模块 TX/RX（交叉连接），GND 共地 |
| PA6 / PA7 / PB0 | 绿 / 红 / 蓝 LED（串限流电阻） |
| 3.3V / GND | MPU6050、OLED、USB-TTL 共地 |

> MPU6050 的 AD0 接 GND 时地址 0x68，接 VCC 时 0x69，驱动两种都尝试。

## CubeMX 配置说明

工程文件 `levelformal.ioc`，MCU 选 **STM32F103C8Tx（LQFP48）**，生成工具链 **MDK-ARM V5**，库版本 STM32Cube FW_F1。

| 配置项 | 设置 |
|---|---|
| 时钟源 | HSE 外部晶振 8MHz（PD0/PD1），PLL ×9 → SYSCLK 72MHz |
| 总线分频 | AHB /1 = 72MHz，APB1 /2 = 36MHz，APB2 /1 = 72MHz，Flash 延迟 2WS |
| 调试接口 | SYS → Debug: Serial Wire（SWD） |
| I2C2 | PB10 = SCL、PB11 = SDA，Fast Mode 400kHz，7 位地址 |
| USART2 | 115200 8N1，TX/RX 模式，PA2/PA3 复用推挽 + 浮空输入 |
| DMA | USART2_RX → DMA1 Channel 6，USART2_TX → DMA1 Channel 7，均 **Normal 模式**、优先级 Low |
| NVIC | USART2_IRQn、DMA1_Channel6_IRQn、DMA1_Channel7_IRQn 使能，抢占优先级 5 |
| FreeRTOS | 接口 CMSIS-RTOS V2，heap_4，`configTOTAL_HEAP_SIZE = 8192`，tick 1000Hz，`configMAX_PRIORITIES = 56` |
| 生成的 RTOS 对象 | 任务 `IMU`/`UART`/`DISP`/`defaultTask`、队列 `attitudeQueue`(1)、`uartFrameQueue`(4)、互斥量 `i2cMutex`、二值信号量 `uartTxDone` |
| 系统节拍 | HAL 时基 TIM（`stm32f1xx_hal_timebase_tim.c`），避免与 FreeRTOS 争 SysTick |

> DMA 刻意用 Normal 而不是 Circular：空闲中断里要拿到"本帧长度（Size）"并重新挂起接收，
> 每帧一次 `HAL_UARTEx_ReceiveToIdle_DMA` 更直观，也避免环形缓冲区的覆盖判断。

## FreeRTOS 任务结构

| 任务 | 入口函数 | 优先级 | 栈 | 周期 | 职责 |
|---|---|---|---|---|---|
| IMU | `AppImu_Task` | High(40) | 1KB | 10ms | 初始化 MPU6050 → 零偏校准 → 读六轴原始值 → 互补滤波 → 投递姿态 |
| UART | `AppUart_Task` | AboveNormal(32) | 1KB | 事件驱动 | 等发送完成信号量 → 取一帧 → DMA 原样回传 |
| DISP | `AppDisplay_Task` | Low(8) | 1KB | 50ms | 取最新姿态 → 刷 OLED + 控 LED |
| defaultTask | `StartDefaultTask` | Normal(24) | 512B | — | CubeMX 默认任务，空转（`osDelay(100)`） |

**优先级理由**：IMU 采样对时间最敏感（10ms 周期 + dt 实测），最高；串口回显要求"立即返回"，次之；
OLED 刷屏是纯人眼可见的慢速任务，最低，即使被拖慢也不影响姿态解算。

**任务间通信**

| 对象 | 类型 | 深度/初值 | 作用 |
|---|---|---|---|
| `attitudeQueue` | 消息队列 | 1 | IMU → 显示。100Hz 产、20Hz 消，队列满时生产者先取走旧值再放新值，即"最新值信箱" |
| `uartFrameQueue` | 消息队列 | 4 | 串口中断 → 串口任务。中断里不能做发送，投递后由任务回传 |
| `i2cMutex` | 互斥量 | — | I2C2 被 MPU6050 与 OLED 共享，读传感器与刷屏必须互斥 |
| `uartTxDone` | 二值信号量 | 初值 1 | 表示"发送通道空闲"，`HAL_UART_TxCpltCallback` 里释放 |

**数据流**

```
MPU6050 ─I2C2(mutex)─► IMU任务 ─attitudeQueue(1)─► 显示任务 ─I2C2─► OLED
                                                        └─► LED

PC ─USART2 RX/DMA1_CH6─► 空闲中断 ─uartFrameQueue(4)─► UART任务 ─USART2 TX/DMA1_CH7─► PC
```

## 目录结构

```
levelformal/
├── Core/
│   ├── Inc/                        头文件（app_common.h 存放任务间消息结构体）
│   └── Src/
│       ├── main.c / freertos.c     系统时钟、外设初始化、任务创建
│       ├── app_imu.c               IMU 任务：零偏校准 + 互补滤波 + Yaw 积分
│       ├── app_display.c           显示任务：OLED 排版、角度格式化、LED 控制
│       ├── app_uart.c              串口任务：DMA 接收/发送、帧回显
│       ├── mpu6050.c               MPU6050 驱动（寄存器初始化、14 字节连读）
│       ├── ssd1306.c               SSD1306 OLED 驱动（1KB 显存、8x8 字库）
│       ├── stm32f1xx_*.c           HAL 初始化与中断（CubeMX 生成）
│       └── font8x8.c               ASCII 8x8 点阵字库
├── Drivers/                        STM32F1xx HAL + CMSIS（CubeMX 生成）
├── Middlewares/Third_Party/FreeRTOS  FreeRTOS Kernel（CubeMX 生成）
├── MDK-ARM/                        Keil MDK 工程（levelformal.uvprojx）
│   └── .eide/eide.yml              VSCode + EIDE 插件工程配置
├── levelformal.ioc                 STM32CubeMX 工程文件
├── README.md
└── LICENSE
```

## 串口通信收发流程

**接收（DMA + 空闲中断，天然支持不定长）**

1. `AppUart_Task` 启动时先调用 `AppUart_RxArm()`：
   `HAL_UARTEx_ReceiveToIdle_DMA(&huart2, s_rx_buf, 128)`，随后
   `__HAL_DMA_DISABLE_IT(&hdma_usart2_rx, DMA_IT_HT)` —— **关掉 DMA 半传输中断**，
   否则一帧长数据会在"搬了一半"时就被当成一帧回调，被切成两片。
2. 上位机发数据 → DMA 自动把字节搬进 `s_rx_buf`，CPU 不参与。
3. 总线上连续 1 个字符时间没有新数据 → USART2 触发 **IDLE 中断**，HAL 结束本次 DMA 传输并调用
   `HAL_UARTEx_RxEventCallback(huart, Size)`，`Size` 就是这一帧的实际长度。
4. 回调（中断上下文）里把 `Size` 个字节 `memcpy` 到 `UartFrame_t`，`osMessageQueuePut` 投进
   `uartFrameQueue`（深度 4），紧接着再次 `AppUart_RxArm()` 挂起接收下一帧。
5. 一帧 = 两次总线空闲之间的全部字节，长度任意，不需要帧头帧尾、不需要超时判断。

**发送（DMA + 发送完成信号量）**

1. `AppUart_Task` 循环先 `osSemaphoreAcquire(uartTxDoneHandle, osWaitForever)`，
   拿到"发送通道空闲"才继续（初始值为 1，所以第一帧可以直接发）。
2. 从队列取一帧 → `HAL_UART_Transmit_DMA(&huart2, frame.data, frame.len)`，由 DMA 逐字节发出。
3. 发送完成中断 → `HAL_UART_TxCpltCallback` → `osSemaphoreRelease(uartTxDoneHandle)` → 循环。
4. 全程不改动数据，**回传内容与接收内容逐字节一致**；收发走不同 DMA 通道，可全双工。

**时序**：从最后一个字节到达，到回传的第一个字节发出，实测 1~2ms（主要花在中断 → 队列 → 任务切换）。

**限制**：单帧最大 128 字节（`APP_UART_RX_BUF_SIZE`，超出会 DMA 溢出丢数据）；
突发的连续多帧超过队列深度 4 时会丢帧。

## IMU 型号、接口、校准与姿态解算

**型号与接口**：MPU6050（六轴 MEMS，裸芯片模块，不含任何内置姿态解算），I2C2 @400kHz，
`MPU6050_Init` 先探测 0x68 / 0x69 两个地址（AD0 电平），读回 `PWR_MGMT_1` 校验后才算初始化成功。

**关键寄存器**（`mpu6050.c`）

| 寄存器 | 值 | 含义 |
|---|---|---|
| `PWR_MGMT_1` | 0x01 | 解除睡眠，时钟源选陀螺 X 轴 PLL（比内部 RC 稳定） |
| `SMPLRT_DIV` | 0x09 | 采样率 1000/(1+9) = 100Hz，与 10ms 任务周期匹配 |
| `CONFIG` | 0x03 | DLPF 数字低通：加速度 44Hz / 陀螺 42Hz，延迟约 4.9ms，显著压噪声 |
| `GYRO_CONFIG` | 0x00 | 陀螺 ±250°/s，131 LSB/(°/s) |
| `ACCEL_CONFIG` | 0x00 | 加速度 ±2g，16384 LSB/g |

`MPU6050_ReadRaw` 从 `ACCEL_XOUT_H` 起一次连读 14 字节（ax/ay/az/temp/gx/gy/gz），按大端拼成 int16。

**零偏校准**：板子静止时陀螺理论输出 0，实际有恒定小偏置（规格书 25°C 初始容差 ±20°/s）。
`Imu_CalibrateGyro` 采 200 次、每 5ms 一次求平均，噪声被压到 1/√200 ≈ 1/14，
得到的 `s_bias_gx/gy/gz`（单位 °/s）在每拍读取后减去。**校准期间板子必须静止**，否则平均出来的是错的。

**姿态解算**

```
roll_acc  = atan2(ay, az)                         // 加速度计静态倾角
pitch_acc = atan2(-ax, sqrt(ay² + az²))           // 用 sqrt 约掉 g 的模长，与 roll 解耦

roll  = α · (roll  + gx·dt) + (1-α) · roll_acc    // 互补滤波，α = 0.98
pitch = α · (pitch + gy·dt) + (1-α) · pitch_acc
yaw  += gz · dt                                    // 无绝对参考，纯积分
```

- `α = 0.98` 对应时间常数 τ = dt/(1-α) ≈ 0.5s：短时间信陀螺（动态好），长时间靠加速度计的绝对重力基准拉回，Roll/Pitch 不会长期漂移。
- `dt` 用 `HAL_GetTick()` 实测而非固定 10ms，并夹在 [1ms, 100ms]：显示任务占着 I2C 总线时 IMU 采样会抖动，实测 `dt` 保证积分时间正确；限幅防止断点调试造成单拍积分暴走。
- Yaw 没有磁力计做绝对参考，只能积分，会缓慢漂移。代码里的 ±180° 绕圈**只是防止数值无限累加**，不是抑制漂移。

## OLED 显示说明

| 项 | 说明 |
|---|---|
| 屏幕 | SSD1306 128×64，I2C 地址 0x3C，与 MPU6050 共用 I2C2 |
| 显存 | 1KB（8 页 × 128 列），`SSD1306_Clear` → 画 → `SSD1306_UpdateScreen` 一次整屏刷新 |
| 字库 | 8×8 ASCII 点阵（`font8x8.c`），一个字符 8 像素宽 |
| 刷新周期 | 50ms（IMU 的 1/5）。整屏 1KB 经 400kHz I2C 传输约 23ms，刷太快会长时间占住总线、拖慢 IMU 采样 |
| 数据来源 | `attitudeQueue` 深度 1，取到的永远是最新姿态；没有新数据就沿用上一帧 |

**页面布局**

| y 坐标 | 内容 |
|---|---|
| 0 | 标题 `IMU Attitude` |
| 16 | `Roll:  12.35°` |
| 32 | `Pitch: -5.67°` |
| 48 | `Yaw:   89.01°` |

- 角度行间距 16 像素（而不是 8），是因为度符号画在文字上方 3 像素处，需要留白。
- 度符号不是字符，而是 `DrawDegree` 手画的小圆点，x 坐标按 `strlen(line)*8 + 3` 动态计算，紧跟在数字后面。
- 格式化用 `FormatAngle`：`v = (int32_t)(angle*100 + 0.5)` 做成"百分之一度"的定点整数，再 `sprintf(buf, "%s%c%d.%02d", ...)` 拼出两位小数。**不用 `%.2f` 是为了不链接浮点格式化库**，省几 KB Flash。
- 正数符号位输出空格（`Roll:  12.35`），负数输出 `-`，这样数字不会左右跳。
- 异常状态页：无数据 `waiting...`；初始化失败 `IMU ERROR! / check wiring`（同时红灯常亮）；校准中 `Calibrating... / keep it still`。

## 编译 / 下载

**Keil MDK5**（推荐，仓库自带工程）
1. 打开 `MDK-ARM/levelformal.uvprojx`
2. F7 编译，F8 下载（需 ARM Compiler 5；在 Keil 里装好 STM32F1xx DFP 器件包）
3. 下载后不运行的话，检查 `Debug → Settings → Flash Download` 是否勾选 Reset and Run

**VSCode + EIDE 插件**
1. 打开 `MDK-ARM/` 目录，EIDE 读取 `.eide/eide.yml`
2. 选好 ARMCC(AC5) 工具链后编译、烧录

**STM32CubeMX**：`levelformal.ioc` 可重新生成 `Core/Src/stm32f1xx_*.c` 等 HAL 初始化代码，
注意**不要覆盖** `app_*.c`、`mpu6050.c`、`ssd1306.c` 这些手写文件。

**串口助手**：115200 8N1，任意长度发一帧，STM32 会原样回传（单帧 ≤128 字节）。

## 遇到的问题与解决方法

| # | 问题 / 现象 | 原因 | 解决 |
|---|---|---|---|
| 1 | 静止时 Yaw 每秒涨 0.3° 左右 | MPU6050 的上电零偏（+52 LSB ≈ 0.4°/s） | 上电静止采样 200×5ms 求平均做零偏校准，每拍减去 |
| 2 | Yaw 长时间缓慢漂移，无法归零 | 没有磁力计，航向没有绝对参考；零偏还随温度变化（全温区 ±20°/s） | 只能纯积分 + ±180° 绕圈防溢出；在 README 中明确标注为物理限制 |
| 3 | 陀螺噪声明显偏大，采样点之间相关性高 | 注释写成 `/* ... 。` 少了一个 `*/`，把下面整行 `MPU6050_REG_CONFIG` 的写入语句一起吃进了注释，DLPF 实际没配置（保持 256Hz 带宽） | 补上 `*/`，CONFIG=0x03 生效，噪声与延迟都落到预期值 |
| 4 | 编译报 `struct "<unnamed>" has no field "temp"` | 精简结构体时删掉了 `MPU6050_Raw_t.temp`，但 `MPU6050_ReadRaw` 里仍在赋值 | 把 `int16_t temp;` 加回，同时保留温度读数（后续可做温漂补偿） |
| 5 | 长帧被拆成两段回传 | DMA 半传输中断也会触发 `RxEventCallback` | 在 `AppUart_RxArm` 里关闭 `DMA_IT_HT`，只认 IDLE/满传输 |



## 已知限制

- **Yaw 会漂**：MPU6050 零偏随温度变化（规格书全温区 ±20°/s），没有磁力计就无法长期锁住航向。
- **未做温度补偿**：`MPU6050_ReadRaw` 已经把片上温度读进 `raw.temp`，但校准与解算都没用它。
- **运行中 I2C 掉线不报警**：只有上电初始化失败才会显示 `IMU ERROR!`；运行中读失败只是丢掉这一拍，屏幕定格在最后一个角度。
- **无静止检测**：没有 ZUPT（零速修正），板子静止时零偏残余仍会被积分。
- **串口单帧 ≤128 字节**：超过会 DMA 溢出丢数据；突发多帧超过队列深度 4 会丢帧。

## 许可证

本项目自写代码采用 [MIT](LICENSE)。第三方组件版权归各自作者：

| 组件 | 许可证 |
|---|---|
| STM32F1xx HAL Driver / CMSIS Device | BSD-3-Clause（STMicroelectronics） |
| CMSIS Core | Apache-2.0（ARM Limited） |
| FreeRTOS Kernel | MIT（Amazon / Richard Barry） |
