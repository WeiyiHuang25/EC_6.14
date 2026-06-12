# STM32 中断与定时器




## 一、中断基础概念

### 1.1 什么是中断？

中断（Interrupt）是 CPU 在执行主程序的过程中，由**外部或内部事件**主动打断当前程序的执行，转去执行一段专门的处理程序（中断服务函数），处理完毕后再返回原程序继续执行的一种机制。

**为什么要用中断？** ：

想象这样一个场景:

你在写作业，你的同学将会来到你家打电动，你要下楼给他开门

我们可以用这种方法实现
```c
void main()
{
    while(1)
    {
        doHomeworkOnce(); // 做一次作业
        if(checkDoor() == FRIEND_COME) // 主动检查门是否被敲，如果是同学则开始打电动
        {
            playVideoGames();
        }
    }
}
```
这样就会产生几个问题:
- 如果 `doHomeworkOnce()` 的执行时间过长，你做一次作业的时间太长，你的朋友就会在门口等很久
- 每完成一次作业就需要下楼检查一次门，会耗费体力(CPU资源)
- 实际程序中，我们可能要响应大量的外部事件，使用轮询会严重影响我们程序的时效性

这显然不是正常人的逻辑

正常人的思路:
```
写作业 <--|
  |------|
  ↓
同学敲门
  |
  ↓
打电动
```
正常人是一直写作业，知道听到朋友敲门，才去开门
但是，对于单片机来说，一般情况下用户程序只有一个入口`__main` 或 `main` ，也就是说程序是线性的，无法自动对外界响应，只能主动轮询

因此我们引入中断这一概念，借助中断，我们可以实现对事件的实时响应


| 特性 | 轮询（Polling） | 中断（Interrupt） |
|------|----------------|-----------------|
| CPU 占用 | 持续查询，浪费资源 | 空闲时执行主程序，事件触发才响应 |
| 响应速度 | 取决于轮询频率，延迟较大 | 事件触发立即响应，速度快 |
| 适用场景 | 简单、实时性要求不高的场合 | 实时性要求高、多任务场景 |

### 1.2 中断的触发与执行流程
中断是一套由软件系统和硬件系统深度结合的响应系统

| 中文 | 解释 |
|--------|--------|
| 中断源 | 引起中断的事件 |
| 中断请求 | 中断源向处理器提出处理的请求 |
| 断点 | 发生中断时被打断程序的暂停点 |
| 中断响应 | 处理器暂停现行程序而转为响应中断请求的过程 |
| 中断服务例程 | 处理中断源的程序 |
| 中断向量表 | 存储中断服务例程地址的内存结构 |
| 中断返回 | 返回断点的过程 |
| 嵌套向量中断控制器 | 管理中断和异常, 仲裁中断的硬件模块 |
---
<br>
中断的处理流程如下：

```
外设产生中断请求
        ↓
NVIC 接收并判断优先级
        ↓
CPU 保存当前程序上下文
        ↓
跳转到中断向量表，获取 ISR 地址
        ↓
执行 ISR
        ↓
清除中断标志位
        ↓
恢复上下文，返回主程序
```

### 1.3 中断优先级与嵌套

如果有两个事件同时触发，我该优先处理哪个呢？

这时候就需要用到 **嵌套中断** 了

- **抢占优先级（Preemption Priority）**：决定高优先级中断能否打断正在执行的低优先级中断。数值越小优先级越高。
- **子优先级（Sub Priority）**：当两个中断的抢占优先级相同时，子优先级高的先执行。
- **自然优先级**：当抢占优先级和子优先级都相同时，按向量表顺序排队。

高优先级的中断会像中断打断常规任务一样打断低优先级中断，等待高优先级中断结束后才继续执行低优先级中断
![NVIC1](./pictures/NVIC1.png)

---

## 二、STM32 中断系统


### 2.1 外部中断 EXTI

EXTI（External Interrupt/Event Controller）是 STM32 用来处理**外部事件触发中断**的模块。

![NVIC2](./pictures/NVIC2.png)

### 2.2 NVIC（嵌套向量中断控制器）

> 具体细节参考 `RM0090` : Section `12` 与 `PM0214` : Section `4.3`

NVIC 是 ARM Cortex-M 内核的一部分，与 CPU 紧密耦合，是 STM32 中断管理的核心。它提供以下特性：

- **可嵌套中断**：高优先级可打断低优先级 ISR
- **向量中断**：硬件自动定位中断入口，无需软件判断
- **动态优先级调整**：可在程序运行中修改优先级
- **极低的中断延迟**：末尾连锁（Tail-Chaining）和迟到（Late Arrival）机制大幅缩短切换时间
- **中断可屏蔽**：可灵活控制每个中断通道的使能与禁用

> 注: 虽然在PM 中，NVIC被写在 `Core Peripheral` 中，但从硬件上看，NVIC是内核的一部分，并不是常见语境的外设，它有一个特殊的翻译 `内核外设`


### 2.3 中断向量表 (Interrupt Vector Table)

中断向量表是一段预先定义好的地址列表，每个地址对应一个中断源的 ISR 入口函数。默认存放于 Flash 起始地址 `0x08000000`。

通过中断向量表，经过NVIC处理后的中断信号可以在IVT中找到对应的函数地址

STM32的中断向量表在代码中储存在启动文件 `startup_*.s`，采用汇编语言

### 2.4 简化的中断模型

正如前文所说，中断的实现是软件于硬件的深度结合，在此，我们将软硬件的过程结合，帮助大家理解
> **注意** 虽然我们在概念上把软件和硬件分开，但我们必须深刻意识到软件是运行在硬件上的，软件的实现实际上是基于硬件的动作

> **注意** 我们在此讨论的是硬件中断，软件中断通路与此有些不同

```
硬件外设
   |
   |    产生中断信号
   V
  EXTI  捕获到外设产生的中断信号，并检测该中断是否启用(掩码)，并置标志位
   |
   |    如果该中断启用
   v
  NVIC  对中断进行仲裁等处理，选择出应该执行的中断号(不是前面的中断信号)
   |
   |    将中断号传给内核
   v
  内核  1.接收到中断号后，保护现场，在IVT中找到中断号所对应ISR并进入中断处理函数
        2.中断函数运行完成后(记得清除中断标志位)，恢复现场，继续执行之前程序

```
>**注意** 根据PM手册，如果不清除标志位，NVIC将不会再对EXTI的该中断响应



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