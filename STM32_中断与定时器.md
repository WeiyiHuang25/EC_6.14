# STM32 中断与定时器

> 本文档为理论教学材料，涵盖 STM32F1 系列的中断系统与定时器原理，辅以代码示例与实验建议。

---

## 一、中断基础概念

### 1.1 什么是中断？

中断（Interrupt）是 CPU 在执行主程序的过程中，由**外部或内部事件**主动打断当前程序的执行，转去执行一段专门的处理程序（中断服务函数），处理完毕后再返回原程序继续执行的一种机制。

**为什么要用中断？** 对比两种工作模式：

| 特性 | 轮询（Polling） | 中断（Interrupt） |
|------|----------------|-----------------|
| CPU 占用 | 持续查询，浪费资源 | 空闲时执行主程序，事件触发才响应 |
| 响应速度 | 取决于轮询频率，延迟较大 | 事件触发立即响应，速度快 |
| 功耗 | 高（CPU 持续工作） | 低（可让 CPU 进入休眠等待中断） |
| 适用场景 | 简单、实时性要求不高的场合 | 实时性要求高、多任务场景 |

### 1.2 中断的触发与执行流程

中断的处理流程如下：

```
外设产生中断请求（标志位置位）
        ↓
NVIC 接收并判断优先级
        ↓
CPU 保存当前程序上下文（自动压栈）
        ↓
跳转到中断向量表，获取 ISR 地址
        ↓
执行中断服务函数（ISR）
        ↓
清除中断标志位
        ↓
恢复上下文，返回主程序
```

### 1.3 中断优先级与嵌套

STM32 使用 **NVIC（嵌套向量中断控制器）** 管理中断优先级。

- **抢占优先级（Preemption Priority）**：决定高优先级中断能否打断正在执行的低优先级中断。数值越小优先级越高。
- **子优先级（Sub Priority）**：当两个中断的抢占优先级相同时，子优先级高的先执行。
- **自然优先级**：当抢占优先级和子优先级都相同时，按向量表顺序排队。

STM32 使用 4 位二进制表示优先级，因此共有 **16 个可编程优先级**（数值 0~15，0 最高）。

通过 `NVIC_PriorityGroupConfig()` 可以将这 4 位划分为不同的抢占/子优先级组合：

| 分组方式 | 抢占优先级位数 | 子优先级位数 | 抢占优先级范围 | 子优先级范围 |
|---------|--------------|-------------|--------------|-------------|
| NVIC_PriorityGroup_0 | 0 位 | 4 位 | — | 0~15 |
| NVIC_PriorityGroup_1 | 1 位 | 3 位 | 0~1 | 0~7 |
| NVIC_PriorityGroup_2 | 2 位 | 2 位 | 0~3 | 0~3 |
| NVIC_PriorityGroup_3 | 3 位 | 1 位 | 0~7 | 0~1 |
| NVIC_PriorityGroup_4 | 4 位 | 0 位 | 0~15 | — |

> **注意**：一旦设定了优先级分组，某一中断的抢占和子优先级取值范围就受到约束。例如分组为 `NVIC_PriorityGroup_2`（2 位抢占 + 2 位子优先级），则抢占优先级范围为 0~3，子优先级范围也为 0~3。

---

## 二、STM32 中断系统

### 2.1 NVIC（嵌套向量中断控制器）

NVIC 是 Cortex-M3 内核的一部分，与 CPU 紧密耦合，是 STM32 中断管理的核心。它提供以下特性：

- **可嵌套中断**：高优先级可打断低优先级 ISR
- **向量中断**：硬件自动定位中断入口，无需软件判断
- **动态优先级调整**：可在程序运行中修改优先级
- **极低的中断延迟**：末尾连锁（Tail-Chaining）和迟到（Late Arrival）机制大幅缩短切换时间
- **中断可屏蔽**：可灵活控制每个中断通道的使能与禁用

### 2.2 外部中断 EXTI

EXTI（External Interrupt/Event Controller）是 STM32 用来处理**外部事件触发中断**的模块。

#### 2.2.1 EXTI 线路映射

STM32F1 系列有 **23 条中断/事件线**，其中 16 条（EXTI0~EXTI15）分别对应 GPIO 引脚：

| EXTI 线路 | GPIO 映射 |
|-----------|-----------|
| EXTI0 | PA0 / PB0 / PC0 / PD0 / PE0 |
| EXTI1 | PA1 / PB1 / PC1 / PD1 / PE1 |
| EXTI2 | PA2 / PB2 / PC2 / PD2 / PE2 |
| ... | ... |
| EXTI15 | PA15 / PB15 / PC15 / PD15 / PE15 |

> **关键点**：同一 EXTI 线路上的多个引脚（如 PA0 和 PB0）**不能同时使用**外部中断功能，需要通过 AFIO 时钟和配置寄存器选择实际使用的引脚。

此外，EXTI 还连接了特殊事件源：
- EXTI16 → PVD 输出
- EXTI17 → RTC 闹钟
- EXTI18 → USB 唤醒

#### 2.2.2 EXTI 框架与中断/事件区别

```
输入信号
  ↓
边沿检测电路（上升沿/下降沿/双边沿）
  ↓
────┬───────────────────────┬────
    ↓                       ↓
中断路径               事件路径
(EXTI_IMR 控制)        (EXTI_EMR 控制)
    ↓                       ↓
NVIC 控制器          脉冲发生器
(触发 ISR)           (触发其他外设)
```

- **中断**：信号经 NVIC 触发 CPU 执行中断服务函数 → 软件级响应
- **事件**：信号绕过 CPU，直接触发其他外设（如 ADC、DMA）→ 硬件级联动，响应更快且无需软件介入

#### 2.2.3 EXTI 关键寄存器

| 寄存器 | 作用 |
|--------|------|
| EXTI_IMR | 中断屏蔽寄存器 — 开启/屏蔽各线路的中断请求 |
| EXTI_EMR | 事件屏蔽寄存器 — 开启/屏蔽各线路的事件请求 |
| EXTI_RTSR | 上升沿触发选择寄存器 |
| EXTI_FTSR | 下降沿触发选择寄存器 |
| EXTI_PR | 挂起寄存器 — 记录哪个线路发生了中断（需软件清除） |

### 2.3 中断向量表

中断向量表是一段预先定义好的地址列表，每个地址对应一个中断源的 ISR 入口函数。存放于 Flash 起始地址 `0x08000000`（可重定位至 SRAM）。

STM32F1 的向量表包含：
- 第 1 项：栈顶指针（MSP）
- 第 2 项：复位向量（Reset_Handler）
- 其后：NMI、HardFault、SVC、PendSV、各外设中断（TIMx、USARTx、EXTIx 等）

### 2.4 中断服务函数（ISR）的编写规范

1. **函数名必须与启动文件中的向量表一致**，例如：
   - `void TIM2_IRQHandler(void)` — TIM2 全局中断
   - `void EXTI0_IRQHandler(void)` — EXTI0 中断
   - `void USART1_IRQHandler(void)` — USART1 中断

2. **在 ISR 开头判断并清除标志位**：
   ```c
   void TIM2_IRQHandler(void) {
       if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
           // 业务逻辑
           TIM_ClearITPendingBit(TIM2, TIM_IT_Update); // 清除标志位
       }
   }
   ```

3. **ISR 应尽量短小高效**，避免在中断中执行耗时操作（如大循环、打印、延时）。

4. **注意中断标志位的清除**：若不清除，退出 ISR 后会再次进入，形成死循环。

---

## 三、定时器概述

### 3.1 为什么需要定时器？

定时器（Timer）是 STM32 中最常用的外设之一，能够精确控制时间：定时触发中断、PWM 输出、测量外部信号、生成精确延时等。相比软件延时（循环空转），定时器不占用 CPU 资源，精度高且可与 CPU 并行工作。

### 3.2 STM32 定时器分类

STM32F1 系列共有 8 个定时器，分为三类：

| 类型 | 定时器 | 所在总线 | 主要功能 |
|------|-------|---------|---------|
| **基本定时器** | TIM6、TIM7 | APB1 | 定时中断、触发 DAC |
| **通用定时器** | TIM2、TIM3、TIM4、TIM5 | APB1 | 定时、输入捕获、输出比较、PWM、编码器接口、主从触发 |
| **高级定时器** | TIM1、TIM8 | APB2 | 通用定时器全部功能 + 死区生成、互补输出、刹车输入、重复计数器 |

> **注意**：APB1 最大频率为 36MHz，APB2 最大频率为 72MHz。当 APB1/2 的预分频系数不为 1 时，定时器时钟频率会翻倍（即 2 倍频）。

### 3.3 定时器时钟源

通用定时器的时钟来源有四种：

1. **内部时钟（CK_INT）**：最常用，直接使用 APB 时钟倍频后的频率
2. **外部时钟模式 1**：通过 TIx（CH1/CH2/CH3/CH4）引脚输入
3. **外部时钟模式 2**：通过 ETR 引脚输入
4. **内部触发输入（ITRx）**：由其他定时器触发

---

## 四、定时器核心功能

### 4.1 计数器模式

定时器的核心是一个 16 位自动重装载计数器（TIMx_CNT），支持三种计数模式：

#### 向上计数（Up Count）
- 计数器从 0 开始，每来一个时钟脉冲 +1
- 计数至 ARR（自动重装载值）时触发上溢事件，计数器归 0 重新开始

#### 向下计数（Down Count）
- 计数器从 ARR 值开始，每来一个时钟脉冲 -1
- 计数至 0 时触发下溢事件，计数器回到 ARR 重新开始

#### 中央对齐模式（Center-Aligned）
- 计数器从 0 向上计数至 ARR-1，触发上溢；然后从 ARR 向下计数至 1，触发下溢
- 交替执行，常用于电机控制（SVPWM）等需要中间对齐波形的场景

### 4.2 预分频器（Prescaler）与自动重装载

- **PSC（预分频器）**：对输入时钟进行分频，分频系数范围 1~65535。分频后的时钟驱动计数器。
- **ARR（自动重装载寄存器）**：计数器达到此值时触发更新事件。

**定时时间计算公式**：

```
T = (ARR + 1) × (PSC + 1) / T_timer_clock

其中 T_timer_clock = 72MHz（APB2 不分频时）
```

**示例**：要产生 1ms 定时（72MHz 时钟，PSC=7199，ARR=9）
```
T = (9 + 1) × (7199 + 1) / 72,000,000 = 10 × 7200 / 72,000,000 = 1ms
```

### 4.3 捕获/比较通道

每个通用定时器有 **4 个独立通道**（CH1~CH4），每个通道可独立配置为：

#### 输入捕获（Input Capture）
- 检测引脚上的边沿跳变（上升沿/下降沿）
- 将当前计数器 CNT 的值锁存到 CCRx 寄存器
- 用于测量脉冲宽度和频率

#### 输出比较（Output Compare）
- 将计数器 CNT 与 CCRx 比较
- 匹配时翻转输出引脚电平或触发中断
- 用于生成精确 timing 信号

#### PWM 生成（PWM Mode）
- 是输出比较模式的特例，输出固定频率的脉宽调制信号
- 可配置 PWM1 和 PWM2 两种模式

### 4.4 更新事件与中断

每当计数器上溢/下溢（或通过软件）时，会产生一个**更新事件（Update Event）**，可触发：
- 更新中断（Update Interrupt）— 最常用
- 更新 DMA 请求
- 触发其他外设（如 DAC）

> **注意**：修改 PSC、ARR 或通过软件触发更新时，新值可能不会立即生效（取决于自动重装载预装载使能状态）。

---

## 五、中断与定时器的结合应用

### 5.1 定时器溢出中断（TIMx_UP）

最基本的用法——定时器计数器溢出时触发中断，用于精确计时任务。

**配置步骤**：
1. 使能 TIMx 时钟（如 `RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE)`）
2. 配置时基单元（PSC、ARR、计数模式）
3. 使能更新中断（`TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE)`）
4. 配置 NVIC 使能对应中断通道
5. 启动定时器（`TIM_Cmd(TIM2, ENABLE)`）
6. 在 `TIM2_IRQHandler()` 中处理定时任务

**典型代码**：
```c
void TIM2_Init(void) {
    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);

    TIM_InitStructure.TIM_Prescaler = 7200 - 1;    // 72MHz / 7200 = 10kHz
    TIM_InitStructure.TIM_Period = 10000 - 1;       // 10kHz / 10000 = 1Hz（1秒）
    TIM_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM2, &TIM_InitStructure);

    TIM_ITConfig(TIM2, TIM_IT_Update, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = TIM2_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 0;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    TIM_Cmd(TIM2, ENABLE);
}

void TIM2_IRQHandler(void) {
    if (TIM_GetITStatus(TIM2, TIM_IT_Update) == SET) {
        // 定时任务：翻转 LED、执行传感器采集等
        LED = !LED;
        TIM_ClearITPendingBit(TIM2, TIM_IT_Update);
    }
}
```

### 5.2 PWM 输出与比较中断

定时器可输出 PWM 波形，通过调节 CCRx 值改变占空比，常用于电机调速、灯光调光、呼吸灯等场景。

**PWM 占空比控制逻辑**：

| 模式 | CNT < CCRx | CNT ≥ CCRx |
|------|-----------|------------|
| PWM1 | 输出高电平 | 输出低电平 |
| PWM2 | 输出低电平 | 输出高电平 |

**呼吸灯示例**：
```c
void PWM_Init(uint16_t arr, uint16_t psc) {
    GPIO_InitTypeDef GPIO_InitStructure;
    TIM_TimeBaseInitTypeDef TIM_InitStructure;
    TIM_OCInitTypeDef TIM_OCInitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

    // GPIO 复用推挽输出
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6;         // TIM3_CH1 → PA6
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    // 时基单元
    TIM_InitStructure.TIM_Period = arr;
    TIM_InitStructure.TIM_Prescaler = psc;
    TIM_InitStructure.TIM_CounterMode = TIM_CounterMode_Up;
    TIM_TimeBaseInit(TIM3, &TIM_InitStructure);

    // PWM 通道
    TIM_OCInitStructure.TIM_OCMode = TIM_OCMode_PWM1;
    TIM_OCInitStructure.TIM_OutputState = TIM_OutputState_Enable;
    TIM_OCInitStructure.TIM_Pulse = 0;                // 初始占空比 0
    TIM_OCInitStructure.TIM_OCPolarity = TIM_OCPolarity_Low;
    TIM_OC1Init(TIM3, &TIM_OCInitStructure);

    TIM_OC1PreloadConfig(TIM3, TIM_OCPreload_Enable);
    TIM_ARRPreloadConfig(TIM3, ENABLE);
    TIM_Cmd(TIM3, ENABLE);
}

// 主循环中改变占空比实现呼吸灯
uint16_t ccr = 0;
uint8_t dir = 0;
while (1) {
    dir ? (ccr--) : (ccr++);
    if (ccr >= 500) dir = 1;
    if (ccr <= 0)   dir = 0;
    TIM_SetCompare1(TIM3, ccr);
    delay_ms(5);
}
```

### 5.3 输入捕获测量脉冲宽度

输入捕获用于测量外部信号的脉冲宽度或频率。原理：在边沿跳变时将计数器 CNT 的值锁存到 CCRx，通过两次捕获的差值计算时间。

**测量方法**（以测量高电平脉宽为例）：
1. 配置通道为**上升沿捕获** → t1 时刻锁存 CNT 到 CCRx，立即切换为下降沿捕获
2. 下降沿到来时 → t2 时刻再次锁存
3. 脉宽 = (CCR2 - CCR1) × 定时器周期

**配置步骤**：
```c
// 以 TIM5_CH1（PA0）测量脉冲宽度为例
RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM5, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

// GPIO 配置为下拉输入
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_0;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPD;
GPIO_Init(GPIOA, &GPIO_InitStructure);

// 时基单元配置
TIM_TimeBaseInitStructure.TIM_Prescaler = 7200 - 1;   // 10kHz 计数频率
TIM_TimeBaseInitStructure.TIM_Period = 0xFFFF;        // 最大计数量程
TIM_TimeBaseInitStructure.TIM_CounterMode = TIM_CounterMode_Up;
TIM_TimeBaseInit(TIM5, &TIM_TimeBaseInitStructure);

// 输入捕获配置
TIM_ICInitStructure.TIM_Channel = TIM_Channel_1;
TIM_ICInitStructure.TIM_ICPolarity = TIM_ICPolarity_Rising;  // 先捕获上升沿
TIM_ICInitStructure.TIM_ICSelection = TIM_ICSelection_DirectTI;
TIM_ICInitStructure.TIM_ICPrescaler = TIM_ICPSC_DIV1;
TIM_ICInitStructure.TIM_ICFilter = 0x0;
TIM_ICInit(TIM5, &TIM_ICInitStructure);

TIM_Cmd(TIM5, ENABLE);
```

### 5.4 定时器触发 ADC/DMA 联动

定时器更新事件可触发 ADC 开始转换，转换完成后 DMA 自动将数据搬移到内存，全程无需 CPU 干预，适用于高速数据采集（如音频采样、传感器数据记录）。

典型配置流程：
1. 配置定时器（TIMx）产生周期性更新事件
2. 配置 ADC 触发源为该定时器
3. 配置 DMA 通道，将 ADC 数据从 DR 寄存器传输到内存数组
4. 启动定时器 → 自动触发 ADC → DMA 自动传输 → 循环覆盖

---

## 六、实战练习建议

### 实验一：定时器控制 LED 闪烁

**目标**：使用 TIM3 产生 1Hz 更新中断，控制 LED 每秒翻转一次。

**要点**：
- 配置 PSC = 7200-1，ARR = 10000-1（1s 定时）
- 在 ISR 中翻转 GPIO 引脚电平
- 观察 LED 闪烁频率是否符合预期

---

### 实验二：外部中断按键响应

**目标**：使用 EXTI 捕获按键按下事件，触发 LED 状态切换。

**要点**：
- 按键 GPIO 配置为上拉输入模式
- 通过 AFIO 配置 GPIO 与 EXTI 线路的映射关系
- 配置 EXTI 下降沿触发（按键按下时为低电平）
- NVIC 配置优先级，在 ISR 中清除 EXTI_PR 标志位

**配置步骤总结**：
```c
// 1. 使能时钟
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOE | RCC_APB2Periph_AFIO, ENABLE);

// 2. GPIO 配置
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_4;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
GPIO_Init(GPIOE, &GPIO_InitStructure);

// 3. AFIO 映射
GPIO_EXTILineConfig(GPIO_PortSourceGPIOE, GPIO_PinSource4);

// 4. EXTI 配置
EXTI_InitStructure.EXTI_Line = EXTI_Line4;
EXTI_InitStructure.EXTI_Mode = EXTI_Mode_Interrupt;
EXTI_InitStructure.EXTI_Trigger = EXTI_Trigger_Falling;
EXTI_InitStructure.EXTI_LineCmd = ENABLE;
EXTI_Init(&EXTI_InitStructure);

// 5. NVIC 配置
NVIC_InitStructure.NVIC_IRQChannel = EXTI4_IRQn;
NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 2;
NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
NVIC_Init(&NVIC_InitStructure);

// 6. ISR
void EXTI4_IRQHandler(void) {
    if (EXTI_GetITStatus(EXTI_Line4) == SET) {
        LED = !LED;                           // 切换 LED 状态
        EXTI_ClearITPendingBit(EXTI_Line4);   // 清除中断标志
    }
}
```

---

### 实验三：PWM 输出控制呼吸灯

**目标**：使用 TIM3_CH1 输出 PWM，调节占空比实现 LED 呼吸灯效果。

**要点**：
- GPIO 配置为复用推挽输出
- PWM 模式选择 PWM1
- 在主循环中渐进增减 CCR 值，观察 LED 亮度平滑变化

---

### 实验四：输入捕获测量信号频率

**目标**：使用 TIM5_CH1 捕获外部方波信号频率，计算并通过串口打印。

**要点**：
- 配置定时器为输入捕获模式，上升沿触发
- 在捕获中断中记录两次上升沿的 CCR 值差
- 根据定时器时钟频率计算信号周期和频率
- 通过 USART 上传测量结果

---

## 七、常用寄存器速查

### NVIC 相关

| 寄存器 | 作用 |
|--------|------|
| NVIC->ISER[0/1] | 中断使能 |
| NVIC->ICER[0/1] | 中断清除（关闭） |
| NVIC->IPR[0~7] | 中断优先级（8 位/中断，4 位有效） |

### EXTI 相关

| 寄存器 | 作用 |
|--------|------|
| EXTI->IMR | 中断屏蔽 |
| EXTI->EMR | 事件屏蔽 |
| EXTI->RTSR | 上升沿触发 |
| EXTI->FTSR | 下降沿触发 |
| EXTI->PR | 挂起标志（写 1 清除） |

### 定时器相关

| 寄存器 | 作用 |
|--------|------|
| TIMx->PSC | 预分频值 |
| TIMx->ARR | 自动重装载值 |
| TIMx->CNT | 当前计数值 |
| TIMx->CCR1~4 | 捕获/比较寄存器 |
| TIMx->DIER | DMA/中断使能寄存器 |
| TIMx->SR | 状态寄存器 |
| TIMx->EGR | 事件生成寄存器 |

---

## 八、学习建议

1. **先理解概念，再动手实践**：中断和定时器的配置步骤较多，建议先从简单的定时器溢出中断入手，理解时基单元和 NVIC 的配合关系。

2. **善用调试工具**：在 ISR 中设置断点，观察中断何时触发、计数器如何变化。

3. **从现象到原理**：遇到不预期的结果（如定时不准、中断不触发）时，先检查时钟配置、NVIC 优先级分组、中断标志清除等常见问题。

4. **逐步扩展功能**：在基本定时中断基础上，逐步加入 PWM 输出、输入捕获，最终实现定时器→ADC→DMA 的联动，形成完整的技术栈。

---

*本文档基于 STM32F1 系列（标准库）编写，适用于正点原子、野火、铁头等主流开发板。如使用 HAL 库或 F4/F7 系列，API 略有不同，但核心原理相通。*