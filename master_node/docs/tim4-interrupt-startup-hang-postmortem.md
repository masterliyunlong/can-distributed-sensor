# STM32 + FreeRTOS: TIM4 中断缺失导致启动卡死复盘

## 一句话描述

在 STM32F103 主节点中启用 FreeRTOS/队列测试时，程序烧录后 OLED 没有任何显示，表现得像“系统卡死”。最终定位到 HAL 将 TIM4 配置为 1 ms 时基并使能了中断，但工程遗漏 `TIM4_IRQHandler()`；首次 TIM4 更新中断进入启动文件的弱默认处理函数后无限循环。

## 项目背景

- 芯片：STM32F103
- 软件：HAL + FreeRTOS
- 主节点功能：ADC、BH1750、DS18B20 数据经 FreeRTOS 队列汇总，再由 OLED 任务显示。
- HAL 时基：TIM4，周期 1 ms。

## 故障现象

1. 主节点下载程序后，OLED 完全不显示，调试现象像死机。
2. 从节点运行正常。
3. 问题最初出现在引入 FreeRTOS 队列期间，因此一开始怀疑队列创建、任务栈或调度器配置。
4. 实际上，故障发生在任务和 OLED 显示逻辑正常运行之前；这说明“引入队列”与“根因”只是时间上相关，并非因果关系。

## 排查过程

### 1. 先排除 GPIO 复用冲突

- OLED 软件 I2C 使用 `PB8/PB9`。
- CAN 使用 `PA11/PA12`。

两者没有复用同一组 GPIO，因此排除 CAN 重映射抢占 OLED 引脚的可能。

### 2. 核实 FreeRTOS 基础路径

- Keil 工程已包含 `app_sensor.c`、`tasks.c`、`queue.c`、`heap_4.c` 和 Cortex-M3 port。
- `SVC_Handler`、`PendSV_Handler` 由 FreeRTOS port 提供。
- `SysTick_Handler` 在调度器启动后调用 `xPortSysTickHandler()`。

这些配置没有发现导致“上电后立即死机”的直接问题。

### 3. 对比主从节点中断文件

主节点与正常从节点使用同一套 TIM4 HAL 时基文件。该文件在 `HAL_Init()` 中完成如下动作：

```c
status = HAL_TIM_Base_Start_IT(&htim4);
HAL_NVIC_EnableIRQ(TIM4_IRQn);
```

但从节点的 `stm32f1xx_it.c` 有：

```c
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}
```

主节点缺少这个函数。

### 4. 根据向量表确认 CPU 实际去向

启动文件的中断向量表将 TIM4 中断指向 `TIM4_IRQHandler`。当应用层未定义该函数时，链接器使用启动文件中的弱默认实现；该默认实现会进入无限循环。

因此执行链路为：

```text
HAL_Init()
  -> HAL_InitTick()
  -> TIM4 启动并使能更新中断
  -> 约 1 ms 后 TIM4 更新事件
  -> TIM4_IRQHandler（弱默认实现）
  -> 无限循环
```

这解释了为什么 OLED 在初始化、写入首屏内容之前就没有显示。

## 根因

**HAL 时基和中断服务函数配置不完整。**

工程采用 `stm32f1xx_hal_timebase_tim.c`，该文件让 TIM4 产生 HAL 的 1 ms tick；但主节点没有在中断向量对应的应用代码中实现 `TIM4_IRQHandler()`，导致第一次 TIM4 中断进入弱默认死循环。

这不是 FreeRTOS 队列 API 阻塞造成的。即使暂时不创建任何任务，只要 TIM4 中断已经被 HAL 启动，缺少处理函数仍会导致卡死。

## 修复

在 `Core/Src/stm32f1xx_it.c` 中加入 TIM4 的中断服务函数：

```c
void TIM4_IRQHandler(void)
{
    HAL_TIM_IRQHandler(&htim4);
}
```

`HAL_TIM_IRQHandler()` 会清除更新中断标志，并进一步调用 `HAL_TIM_PeriodElapsedCallback()`；后者中已有 `HAL_IncTick()`，所以 HAL 的时间基准得以正常递增。

## 验证结果

- TIM4 中断不再进入弱默认死循环。
- 主节点可继续执行初始化和 FreeRTOS 调度。
- OLED 能正常显示，后续再单独验证队列、传感器和任务栈。

## 面试表达参考

> 我在一个 STM32F103 的 FreeRTOS 分布式采集项目中遇到过主节点下载后 OLED 全黑的问题。当时刚加了队列，直觉上怀疑是队列或任务栈，但我没有只围绕 FreeRTOS 查。我先核对了引脚复用，再比较主从节点的启动和中断文件。最后发现工程使用 TIM4 做 HAL 的 1 ms 时基，HAL 已经启动并使能 TIM4 中断，但主节点漏掉了 `TIM4_IRQHandler`。第一次定时器中断进入启动文件的弱默认死循环，所以程序在 OLED 显示和任务调度前就停住了。补上中断服务函数并调用 `HAL_TIM_IRQHandler(&htim4)` 后恢复正常。这个问题让我形成了一个习惯：遇到“启动即死机”，除了应用任务，也要检查时基外设是否启用、NVIC 是否使能，以及向量表中的每个已启用中断是否有正确的 ISR。

## 可延伸的工程改进

- 使用 CubeMX 重新生成代码或增加启动自检，避免启用外设中断却遗漏 ISR。
- 在 HardFault 和 `Error_Handler()` 中增加串口、LED 或调试寄存器信息，缩短定位时间。
- FreeRTOS 项目启用 `configCHECK_FOR_STACK_OVERFLOW` 和 `vApplicationMallocFailedHook()`，将任务栈不足和堆分配失败与中断问题区分开。
- 代码评审时将“启用的 IRQ、NVIC 配置、ISR 实现”作为一组核对项。

## 第二个问题：调度器启动前读取 FreeRTOS 队列导致 CAN 中断被屏蔽

### 一句话描述

在主节点把 CAN 接收从轮询缓存改成 FreeRTOS 队列后，OLED 只能显示初始化时写入的
`rx_id:`、`rx_dlc:` 和 `rx_data:`，始终没有 ID、DLC 和数据。最终定位为：
**调度器尚未启动时，裸机主循环调用了 `xQueueReceive()`，导致 CAN RX0 中断被永久屏蔽。**

### 故障现象

- 从节点仍然周期性发送标准帧 `0x180`。
- 主节点的三个 OLED 标题可以显示，说明 `OLED_Init()` 和后续初始化已经执行。
- `CanRxQueue_Get(&rx_frame, 0U)` 一直返回 `false`，队列接收计数保持为零。
- 旧的 `CanBasic_TakeReceived()` 轮询/单帧缓存版本可以工作；切换到队列版本后停止更新。

### 定位证据

主节点原来的执行顺序是：

```text
CanRxQueue_Init()
CanBasic_Init()
while (1)
    CanRxQueue_Get(&rx_frame, 0U)
```

队列接收函数内部直接调用：

```c
return xQueueReceive(s_rx_queue, frame, wait_ticks) == pdPASS;
```

工程使用的是 FreeRTOS `ARM_CM3` 端口。该端口在调度器启动前将临界区计数初始化为
`0xaaaaaaaa`，而 `xQueueReceive()` 即使使用 `0U` 的非阻塞等待，也会执行
`taskENTER_CRITICAL()` / `taskEXIT_CRITICAL()`。

项目配置和 CAN 中断优先级为：

```c
#define configMAX_SYSCALL_INTERRUPT_PRIORITY 191  /* 0xB0，逻辑优先级 11 */
HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 11, 0);
```

第一次从空队列读取时，FreeRTOS 将 `BASEPRI` 提升到 `0xB0`。由于调度器还没有启动，
临界区计数不会恢复到零，CAN RX0（优先级 11）随后一直无法进入：

```text
xQueueReceive()
  -> 进入临界区并屏蔽 CAN RX0
  -> USB_LP_CAN1_RX0_IRQHandler() 不执行
  -> HAL_CAN_RxFifo0MsgPendingCallback() 不执行
  -> CanRxQueue_PutFromIsr() 不执行
  -> 队列一直为空
```

因此问题不在 CAN 引脚、`0x180` 过滤器或队列内存分配；队列创建成功后，错误的调用上下文
才是关键。

### 修复方案

将队列读取放到 FreeRTOS 任务中，并在任务创建后启动调度器：

```c
static void CanRxTask(void *argument)
{
    CanRxFrame_t frame;

    (void)argument;
    for (;;) {
        if (CanRxQueue_Get(&frame, portMAX_DELAY)) {
            OLED_ShowHexNum(1, 7, frame.id, 4);
            OLED_ShowNum(2, 9, frame.dlc, 1);

            for (uint8_t i = 0U; i < frame.dlc; i++) {
                OLED_ShowHexNum(4, 1 + i * 3, frame.data[i], 2);
            }
        }
    }
}
```

初始化阶段检查创建结果，然后启动调度器：

```c
if (!CanRxQueue_Init()) {
    Error_Handler();
}

if (!CanBasic_Init()) {
    Error_Handler();
}

if (xTaskCreate(CanRxTask, "CanRx", 256U, NULL, 2U, NULL) != pdPASS) {
    Error_Handler();
}

vTaskStartScheduler();
```

CAN 回调只使用 ISR 安全接口：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_handle)
{
    CanRxQueue_PutFromIsr(can_handle);
}
```

队列暂时为空是正常状态，不能因此调用 `Error_Handler()`；在任务中使用
`portMAX_DELAY` 等待数据即可。普通 `xQueueReceive()` 不应放在调度器启动前的裸机主循环中，
而中断中只能调用带 `FromISR` 后缀的 FreeRTOS API。

### 验证结果

- Keil 工程重新编译通过：`0 Error(s), 0 Warning(s)`。
- 调度器启动后 CAN RX0 中断可以进入 HAL 回调。
- 回调将 `0x180` 帧写入队列，`CanRxTask` 被唤醒并更新 OLED。

### 面试表达参考

> 我在把 CAN 接收改成 FreeRTOS 队列时遇到过 OLED 只有标题、数据始终不更新的问题。开始我检查了 CAN 引脚和过滤器，确认从节点发送的 `0x180` 能匹配。随后发现我虽然使用了 `xQueueReceive(..., 0)`，但调用位置仍然是调度器启动前的裸机 `while(1)`。在 STM32F1 的 ARM_CM3 FreeRTOS 端口中，队列接收即使不等待也会进入临界区；调度器尚未启动时临界区计数还未初始化，退出后 `BASEPRI` 没有恢复，正好把优先级 11 的 CAN RX0 中断屏蔽了，所以回调和入队都不会发生。修复方式是创建专门的 CAN 接收任务，在 `vTaskStartScheduler()` 之后用 `portMAX_DELAY` 读取队列，并在 ISR 中只调用 `xQueueSendFromISR()`。这个问题让我认识到，FreeRTOS API 不仅要区分普通上下文和 ISR 上下文，还要确认调用时调度器是否已经启动。
