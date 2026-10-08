# 从 CAN 入门教程到 FreeRTOS CAN 项目：逐步代码计划

> 起点工程：`C:\Data\All\ncepu\embled\can_prj\freertos-multitask-sensor`。
>
> 目标：一主两从。Slave 1 发送 ADC + BH1750；Slave 2 发送 ADC + DS18B20；Master 用 OLED 显示，并可按键请求两个从节点立即刷新。
>
> 这份文档故意先按你教程的结构写。前 5 天几乎就是教程中 `MyCAN.c` 的 HAL 版；第 6 天才解释为什么 FreeRTOS 需要把“一个接收变量”改成“接收队列”。

---

## 目录

1. [先看清楚：教程代码和本项目怎样对应](#先看清楚教程代码和本项目怎样对应)
2. [开始前的引脚和 CubeMX 设置](#开始前的引脚和-cubemx-设置)
3. [Day 1：复制工程，先不写 CAN 代码](#day-1复制工程先不写-can-代码)
4. [Day 2：教程 01，单板 Loopback](#day-2教程-01单板-loopback)
5. [Day 3：教程 02，两板 Normal 模式](#day-3教程-02两板-normal-模式)
6. [Day 4：教程 02，三板通信](#day-4教程-02三板通信)
7. [Day 5：教程 03 到 08，帧类型和过滤器](#day-5教程-03-到-08帧类型和过滤器)
8. [Day 6：教程 09，中断接收](#day-6教程-09中断接收)
9. [Day 7：教程 10，定时、按键、查询发送](#day-7教程-10定时按键查询发送)
10. [Day 8：Slave 1 发送 ADC 和光照](#day-8slave-1-发送-adc-和光照)
11. [Day 9：Slave 2 发送 ADC 和温度](#day-9slave-2-发送-adc-和温度)
12. [Day 10：Master 接收并显示](#day-10master-接收并显示)
13. [Day 11：周期上报之外，按键请求立即刷新](#day-11周期上报之外按键请求立即刷新)
14. [Day 12：离线和高温告警](#day-12离线和高温告警)
15. [Day 13：按键查询的响应时间和超时](#day-13按键查询的响应时间和超时)
16. [Day 14：测试](#day-14测试)
17. [Day 15：整理](#day-15整理)

---

## 先看清楚：教程代码和本项目怎样对应

你的教程使用标准外设库，文件叫 `MyCAN.c`；你的现有工程使用 HAL，文件是 CubeMX 生成的 `can.c`。两种写法不同，但做的是同一件事。

| 教程中的函数 | 本文中的函数 | 做的事 |
|---|---|---|
| `MyCAN_Init()` | `CanBasic_Init()` | 配过滤器、启动 CAN |
| `MyCAN_Transmit()` | `CanBasic_Send()` | 发一帧标准数据帧 |
| `MyCAN_ReceiveFlag()` | `CanBasic_Receive()` 的返回值 | 判断/读取 FIFO0 的一帧 |
| `CAN_Receive()` | `HAL_CAN_GetRxMessage()` | 从 FIFO0 取一帧 |
| `CAN_Transmit()` | `HAL_CAN_AddTxMessage()` | 放一帧进发送邮箱 |
| `USB_LP_CAN1_RX0_IRQHandler()` | `HAL_CAN_RxFifo0MsgPendingCallback()` | 接收中断入口 |

最重要的区别只有两个：

1. 教程的 `MyCAN.c` 不能复制到你的工程，因为它调用 `CAN_Init()`、`CAN_Transmit()`，而你的工程已经由 HAL 和 CubeMX 管理 CAN。
2. 教程的“一个全局接收帧 + 一个标志位”用于初学没有问题；两个从机每 500 ms 发数据时可能覆盖帧，所以 Day 6 才把它升级为 FreeRTOS 队列。

先不要背后面的代码。每一天只理解当天新增的 1 到 2 个函数。

## 这份教程怎样阅读代码

从现在起，文档里的每一个函数都按同一套顺序写：

1. **函数作用**：它解决什么问题。
2. **谁调用它**：是在 `main()`、普通任务还是中断中调用。
3. **参数和返回值**：数据从哪里来，成功/失败怎样判断。
4. **代码注释**：只对“会影响运行结果”的行解释；`{`、`}` 这类纯语法不重复解释。
5. **本日能看到的现象**：帮助你验证自己没有只“编译通过”。

代码中的两个习惯要先记住：

```c
(void)argument; /* FreeRTOS 任务函数必须有 argument 参数；本任务没用它。 */

if (SomeFunction() != pdPASS) {
    Error_Handler(); /* 创建任务/队列失败不能继续假装系统正常。 */
}
```

`static` 表示变量或函数只在当前 `.c` 文件中可见，避免别的文件误改；`volatile` 表示变量会在中断中被改，编译器每次都必须重新读取它，不能把它长期缓存到寄存器。

---

## 开始前的引脚和 CubeMX 设置

当前 OLED 驱动实际使用 `PB8/PB9`。CAN **必须**用教程默认的 `PA11/PA12`，不要开 CAN Remap。

| 信号 | STM32 引脚 | 说明 |
|---|---|---|
| CAN_RX | PA11 | 接 CAN 收发器的 RXD 输出 |
| CAN_TX | PA12 | 接 CAN 收发器的 TXD 输入 |
| OLED | PB8/PB9 | 已经被 OLED 占用，不能给 CAN |
| Master 按键 1 | PB1 | 查询 Slave 1；按键另一端接 GND，使用内部上拉 |
| Master 按键 2 | PB11 | 查询 Slave 2；按键另一端接 GND，使用内部上拉 |
| UART1 | PA9/PA10 | 看日志 |

CAN_H、CAN_L、GND 三条线三板都要连。总线最两端各一只 120 ohm；断电测 CAN_H 到 CAN_L 约 60 ohm 才继续。

三个 `.ioc` 的 CAN 参数完全相同：

```text
CAN1: PA11/PA12
Mode: Day 2 用 Loopback；Day 3 后用 Normal
APB1: 36 MHz
Prescaler: 9
BS1: 6 TQ
BS2: 1 TQ
SJW: 1 TQ
Bitrate: 36 MHz / [9 x (1 + 6 + 1)] = 500 kbit/s
```

Day 6 打开 `CAN RX FIFO0 interrupt`。在 NVIC 中将 `USB_LP_CAN1_RX0_IRQn` 设置为抢占优先级 11；这是因为回调内会调用 FreeRTOS 的 `xQueueSendFromISR()`。

---

# Day 1：复制工程，先不写 CAN 代码

## 今天只做什么

把已完成的基础工程复制成三份：`master_node`、`slave_node1`、`slave_node2`。每份工程先单独编译、烧录一次。今天不接 CAN，也不改传感器任务。

在每个 CubeMX 工程启用 CAN1，确认引脚为 PA11/PA12，生成 `Core/Inc/can.h` 和 `Core/Src/can.c`。`main.c` 中会多出 `MX_CAN_Init();`。

## 验收

- [ ] 三份工程均能编译。
- [ ] OLED 在 Master 仍正常，证明没有把 CAN 误映射到 PB8/PB9。
- [ ] CAN 尚未调用 `HAL_CAN_Start()`，所以接不接收发器都不影响今天。

---

# Day 2：教程 01，单板 Loopback

教程 01 的逻辑是：“我发一帧，再从自己的 FIFO0 读回来，比较是否一样”。今天只做这件事。

## 新建 `Core/Inc/can_basic.h`

```c
#ifndef CAN_BASIC_H
#define CAN_BASIC_H

#include <stdbool.h>
#include <stdint.h>

/* 初始化过滤器并启动 CAN。成功返回 true，失败返回 false。 */
bool CanBasic_Init(void);

/*
 * 发送一个 11 位标准数据帧。
 * std_id：0x000 到 0x7FF。
 * data：要发送的字节数组。
 * dlc：数据长度，不能超过 8。
 */
bool CanBasic_Send(uint16_t std_id, const uint8_t *data, uint8_t dlc);

/*
 * 轮询 FIFO0 读取一帧。
 * 没有帧或读取失败返回 false；成功时把 ID、长度、数据写到输出变量。
 */
bool CanBasic_Receive(uint16_t *std_id, uint8_t *data, uint8_t *dlc);

#endif
```

## 新建 `Core/Src/can_basic.c`

这就是教程 `MyCAN.c` 的 HAL 版。先逐行看注释，不需要一次记住 HAL 结构体。

```c
#include "can_basic.h"

#include <string.h>     /* memset、memcpy */

#include "can.h"       /* CubeMX 生成的 hcan */

/*
 * 函数作用：配置一个“全接收”过滤器，让所有 CAN 帧都进入 FIFO0。
 * 谁调用：只由 CanBasic_Init() 调用一次。
 * 参数：无；直接使用 CubeMX 生成的全局 hcan。
 * 返回值：HAL 配置成功返回 true，失败返回 false。
 * 为什么 Day 2 用它：此时只验证 CAN 自发自收，不能让过滤器成为额外变量。
 */
static bool CanBasic_ConfigAcceptAllFilter(void)
{
    CAN_FilterTypeDef filter; /* HAL 用这个结构体描述一组硬件过滤器。 */

    /* 先清零，避免结构体中有未初始化的随机值。 */
    memset(&filter, 0, sizeof(filter));

    filter.FilterBank = 0;                         /* STM32F103 的第 0 组过滤器。 */
    filter.FilterMode = CAN_FILTERMODE_IDMASK;     /* 掩码模式。 */
    filter.FilterScale = CAN_FILTERSCALE_32BIT;    /* 使用一个 32 位过滤器。 */
    filter.FilterIdHigh = 0x0000U;                 /* ID 为 0。 */
    filter.FilterIdLow = 0x0000U;
    filter.FilterMaskIdHigh = 0x0000U;             /* 掩码为 0 = 每一位都不比较。 */
    filter.FilterMaskIdLow = 0x0000U;              /* 所以任何帧都能进 FIFO0。 */
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;/* 收到的帧放 FIFO0。 */
    filter.FilterActivation = ENABLE;              /* 真正启用这个过滤器。 */
    filter.SlaveStartFilterBank = 14;              /* F103 单 CAN 也保留这个值。 */

    /* 真正把上面填写的参数写进 CAN 过滤器寄存器。 */
    return HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK;
}

/*
 * 函数作用：完成 CAN 的最后两步——配置过滤器、启动 CAN。
 * 谁调用：main() 的初始化阶段调用一次，必须早于任何收发 CAN 的任务。
 * 参数：无；hcan 是 CubeMX 生成的全局 CAN1 句柄。
 * 返回值：true 表示 CAN 已可收发，false 表示调用者应该进入 Error_Handler()。
 */
bool CanBasic_Init(void)
{
    /* 先配置过滤器，再启动 CAN；顺序不要反过来。 */
    if (!CanBasic_ConfigAcceptAllFilter()) {
        return false;
    }

    /* Loopback 或 Normal 模式由 CubeMX 的 hcan.Init.Mode 决定。 */
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
        return false;
    }
    return true;
}

/*
 * 函数作用：发送一帧“标准 ID + 数据帧”。对应教程 MyCAN_Transmit()。
 * 谁调用：Day 2 的主循环；Day 7 后的普通 FreeRTOS 任务。绝不在中断里调用。
 * std_id：11 位 CAN 标准 ID，合法范围 0x000~0x7FF。
 * data：调用者准备好的数据数组；本函数只读取它，不修改它。
 * dlc：本帧有效字节数，Classic CAN 最大为 8。
 * 返回值：帧已放入硬件发送邮箱返回 true；参数错误、邮箱满或 HAL 出错返回 false。
 */
bool CanBasic_Send(uint16_t std_id, const uint8_t *data, uint8_t dlc)
{
    CAN_TxHeaderTypeDef header; /* HAL 的发送头：ID、帧类型、DLC 都放这里。 */
    uint32_t mailbox;           /* HAL 返回实际使用的 0/1/2 号发送邮箱。 */

    /* 发送前先检查参数，防止越界或发送扩展 ID。 */
    if ((data == NULL) || (std_id > 0x7FFU) || (dlc > 8U)) {
        return false;
    }

    /* header 必须先清零，避免 HAL 读取到未初始化字段。 */
    memset(&header, 0, sizeof(header));
    header.StdId = std_id;                  /* 教程 TxMessage.StdId。 */
    header.IDE = CAN_ID_STD;                /* 教程 CAN_Id_Standard。 */
    header.RTR = CAN_RTR_DATA;              /* 教程 CAN_RTR_Data。 */
    header.DLC = dlc;                       /* 教程 TxMessage.DLC。 */
    header.TransmitGlobalTime = DISABLE;   /* 本项目不使用时间触发通信。 */

    /* 三个发送邮箱都满时，不能继续塞数据。 */
    if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U) {
        return false;
    }

    /* HAL 把这帧放进一个空邮箱，mailbox 返回使用的是哪个邮箱。 */
    return HAL_CAN_AddTxMessage(&hcan, &header, (uint8_t *)data, &mailbox) == HAL_OK;
}

/*
 * 函数作用：用“轮询”方式从 FIFO0 取一帧，对应教程 MyCAN_ReceiveFlag + MyCAN_Receive。
 * 谁调用：Day 2~5 的 while(1) 测试代码。Day 6 开始由中断接收替代它。
 * std_id/data/dlc：都是输出参数，函数成功时写入接收帧信息。
 * 返回值：FIFO 没帧、参数为空、帧类型不符合或 HAL 读取失败都返回 false。
 * 注意：HAL_CAN_GetRxMessage() 成功后，这一帧就从 FIFO0 被取走，不能再读第二次。
 */
bool CanBasic_Receive(uint16_t *std_id, uint8_t *data, uint8_t *dlc)
{
    CAN_RxHeaderTypeDef header; /* HAL 把收到的 ID、IDE、RTR、DLC 写进这里。 */

    if ((std_id == NULL) || (data == NULL) || (dlc == NULL)) {
        return false;
    }

    /* 这就是教程 MyCAN_ReceiveFlag() 的作用：先判断 FIFO0 有没有消息。 */
    if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U) {
        return false;
    }

    /* 这就是教程 CAN_Receive(CAN1, CAN_FIFO0, &RxMessage) 的 HAL 写法。 */
    if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &header, data) != HAL_OK) {
        return false;
    }

    /* Day 2 只接受标准数据帧；其他类型先不研究。 */
    if ((header.IDE != CAN_ID_STD) || (header.RTR != CAN_RTR_DATA) || (header.DLC > 8U)) {
        return false;
    }

    *std_id = (uint16_t)header.StdId; /* 复制给调用者。 */
    *dlc = header.DLC;                /* 告诉调用者 data[] 有多少有效字节。 */
    return true;
}
```

## 在 `Core/Src/main.c` 写 Loopback 测试

先在 CubeMX 把 CAN Mode 设为 `Loopback`。在 `main.c` 的 include 区加入 `#include "can_basic.h"`，然后把下面代码放进 `/* USER CODE BEGIN 2 */`。这是临时测试，不放 FreeRTOS 任务，目的是尽量像教程 01。

```c
/* 发送数组：前两个字节是固定帧头，Byte2~3 放递增序号。 */
uint8_t tx_data[8] = {0xA5U, 0x5AU, 0U, 0U, 0x11U, 0x22U, 0x33U, 0x44U};

/* 接收函数会把 FIFO0 中取出的数据写入这三个输出变量。 */
uint8_t rx_data[8];
uint16_t rx_id;
uint8_t rx_dlc;

/* 1000 次 Loopback 的计数器，同时写入发送帧。 */
uint16_t sequence = 0U;

/* 必须先初始化过滤器、启动 CAN，后面才能收发。 */
if (!CanBasic_Init()) {
    Error_Handler();
}

/* 只做 1000 次；通过后停止，避免初学时无限刷串口。 */
while (sequence < 1000U) {
    tx_data[2] = (uint8_t)(sequence >> 8);  /* 序号高字节。 */
    tx_data[3] = (uint8_t)sequence;         /* 序号低字节。 */

    if (!CanBasic_Send(0x321U, tx_data, 8U)) {
        Error_Handler();
    }

    /* Loopback 下发送的帧会回到自己的 FIFO0；最多等 20 ms。 */
    uint32_t start = HAL_GetTick();
    while (!CanBasic_Receive(&rx_id, rx_data, &rx_dlc)) {
        /* HAL_GetTick() 的单位是 ms；超过 20 ms 就认为本帧没收到。 */
        if ((HAL_GetTick() - start) >= 20U) {
            Error_Handler();
        }
    }

    /* ID、长度、8 个数据字节必须全部相同，才证明 Loopback 正确。 */
    if ((rx_id != 0x321U) || (rx_dlc != 8U) || (memcmp(tx_data, rx_data, 8U) != 0)) {
        Error_Handler();
    }

    sequence++;
}
```

这里用到了 `memcmp()`，所以 `main.c` 还要 include `<string.h>`。串口打印可以每 100 帧加一次，但先跑通后再加，避免第一个错误被打印干扰。

## 今天为什么这样写

- `CanBasic_Send()` 对应教程的 `MyCAN_Transmit()`。
- `CanBasic_Receive()` 对应教程的 `MyCAN_ReceiveFlag() + MyCAN_Receive()`。
- 先配置全接收过滤器，是为了不要让“过滤器写错”干扰 Loopback。
- 不接 CAN_H/CAN_L 也能通过，因为 Loopback 不经过收发器。

## 验收

- [x] 1000 帧发送、接收、比较均通过。
- [x] 任何一帧 ID、DLC 或 8 字节不同时进入 `Error_Handler()`。
- [x] 此时不要进入 Day 3。

---

# Day 3：教程 02，两板 Normal 模式

今天**不增加新函数**，继续使用 Day 2 的 `CanBasic_Init/Send/Receive`。

1. Master 和 Slave 1 的 CubeMX CAN Mode 都改成 `Normal`。
2. 接两块收发器，CAN_H 对 CAN_H、CAN_L 对 CAN_L、GND 对 GND，两端 120 ohm。
3. Master 每 100 ms 发送 ID `0x301`；Slave 1 收到后原样回 ID `0x302`。

## Master 的临时循环

```c
/* Master 的测试请求：Slave 1 收到后必须原样回显这 8 个字节。 */
uint8_t request[8] = {0xA5U, 0x5AU, 0U, 0U, 0U, 0U, 0U, 0U};
uint8_t rx_data[8];       /* 接收 Slave 1 回复的缓存。 */
uint16_t rx_id;           /* 接收帧 ID。 */
uint8_t rx_dlc;           /* 接收帧长度。 */
uint16_t sequence = 0U;   /* 写进 Byte2~3 的请求序号。 */

if (!CanBasic_Init()) {
    Error_Handler();
}

while (1) {
    request[2] = (uint8_t)(sequence >> 8);
    request[3] = (uint8_t)sequence;

    if (!CanBasic_Send(0x301U, request, 8U)) {
        /* 此处先只计数或串口打印，不能直接死循环。 */
    }

    uint32_t start = HAL_GetTick(); /* 100 ms 回复窗口的起点。 */
    bool got_reply = false;         /* 本轮是否已经收到正确回复。 */
    while ((HAL_GetTick() - start) < 100U) {
        if (CanBasic_Receive(&rx_id, rx_data, &rx_dlc)) {
            if ((rx_id == 0x302U) && (rx_dlc == 8U) && (memcmp(request, rx_data, 8U) == 0)) {
                got_reply = true;
                break;
            }
        }
    }

    if (!got_reply) {
        /* timeout_count++; 先记录，不要卡住。 */
    }
    sequence++;
    HAL_Delay(100U);
}
```

## Slave 1 的临时循环

```c
uint8_t rx_data[8]; /* 存放从 FIFO0 读出的 Master 请求数据。 */
uint16_t rx_id;     /* 存放请求帧 ID。 */
uint8_t rx_dlc;     /* 存放请求帧 DLC。 */

if (!CanBasic_Init()) {
    Error_Handler();
}

while (1) {
    /* 没有帧时函数返回 false，while 立即进入下一轮。 */
    if (CanBasic_Receive(&rx_id, rx_data, &rx_dlc)) {
        /* 只回复 Master 的测试 ID，其他帧先忽略。 */
        if ((rx_id == 0x301U) && (rx_dlc == 8U)) {
            (void)CanBasic_Send(0x302U, rx_data, 8U);
        }
    }
}
```

## 验收

- [x] Master 发送一帧，Slave 1 回复一帧。
- [x] 序号和 8 字节内容都一致。
- [x] 连续运行 10 分钟没有超时。
- [x] 如果收不到：先断电测 60 ohm，再查 CAN_H/CAN_L、GND、收发器 RXD/TXD、两边是否都是 500 kbit/s。

---

# Day 4：教程 02，三板通信

今天还是不增加驱动函数，只增加 Slave 2。

| 帧 | 含义 |
|---:|---|
| `0x300` | Master 广播测试请求 |
| `0x302` | Slave 1 回复，Byte 7 固定写 `0x01` |
| `0x303` | Slave 2 回复，Byte 7 固定写 `0x02` |

Slave 2 的循环与 Day 3 Slave 1 完全相同，只改两处：判断 `0x300`，发送 `0x303`，并在发送前写 `rx_data[7] = 0x02U`。Slave 1 写 `0x01U`。

Master 发 `0x300` 后，在 100 ms 内分别等到 `0x302` 和 `0x303`。不能假设谁先到；CAN 会按 ID 仲裁，`0x302` 通常先于 `0x303`。

## 验收

- [x] 三节点运行 15 分钟，两个回复都持续收到。
- [x] 关闭 Slave 2 不影响 Slave 1 回复。
- [x] Slave 1 位于总线中间时不接终端电阻。

---

# Day 5：教程 03 到 08，帧类型和过滤器

今天开始把 Day 2 的“全接收”换成“只收自己要的 ID”。本项目只使用：标准帧、数据帧、DLC=8。教程中的扩展帧和遥控帧只理解，不写进项目。

## 新建 `Core/Inc/can_protocol.h`

```c
#ifndef CAN_PROTOCOL_H
#define CAN_PROTOCOL_H

#include <stdint.h>

/* 最终系统中所有用到的标准 ID。 */
#define CAN_ID_S2_ALARM      0x081U
#define CAN_ID_S1_COMMAND    0x100U
#define CAN_ID_S2_COMMAND    0x101U
#define CAN_ID_S1_TELEMETRY  0x180U
#define CAN_ID_S2_TELEMETRY  0x181U

/* 命令帧 Byte 2 的含义。 */
#define CAN_COMMAND_QUERY_NOW 0x01U

/* 多字节整数统一按大端发送：高字节在前、低字节在后。 */
void CanProtocol_PutU16BE(uint8_t *dst, uint16_t value);
uint16_t CanProtocol_GetU16BE(const uint8_t *src);
void CanProtocol_PutS16BE(uint8_t *dst, int16_t value);
int16_t CanProtocol_GetS16BE(const uint8_t *src);

#endif
```

## 新建 `Core/Src/can_protocol.c`

```c
#include "can_protocol.h"

/*
 * 函数作用：把 uint16_t 拆成两个字节并按“大端”放进 CAN 数据区。
 * 谁调用：Slave 1/2 组装遥测帧时调用。
 * 参数：dst 至少指向两个可写字节；value 是要发送的数。
 * 例子：value=0x1234，发送数组变为 {0x12, 0x34}。
 * 为什么不直接 memcpy(value)：不同 MCU 的内存字节序可能不同，协议必须自己规定。
 */
void CanProtocol_PutU16BE(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8);       /* 高 8 位。 */
    dst[1] = (uint8_t)(value & 0xFFU);    /* 低 8 位。 */
}

/*
 * 函数作用：把 CAN 数据区中两个大端字节还原成 uint16_t。
 * 谁调用：Master 接收到 Slave 1 的 ADC、光照，或 Slave 2 的 ADC 时调用。
 * src：至少指向两个可读字节；返回值就是合并后的无符号数。
 */
uint16_t CanProtocol_GetU16BE(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
}

/*
 * 函数作用：发送带正负号的 int16_t，例如温度 x10。
 * 原理：C 中 int16_t 和 uint16_t 都是 16 位；转换时不改二进制位，只改变解释方式。
 * 例子：-45 的二进制补码经过本函数发送，接收端配合 GetS16BE() 能还原 -45。
 */
void CanProtocol_PutS16BE(uint8_t *dst, int16_t value)
{
    CanProtocol_PutU16BE(dst, (uint16_t)value);
}

/*
 * 函数作用：把两个大端字节还原为带符号 int16_t。
 * 谁调用：Master 读取 Slave 2 的 temperature_x10 时调用。
 */
int16_t CanProtocol_GetS16BE(const uint8_t *src)
{
    return (int16_t)CanProtocol_GetU16BE(src);
}
```

## 把 `can_basic.c` 的过滤器函数替换为下面版本

先不要同时修改发送和接收逻辑，只替换过滤器。这样收不到帧时问题只可能在过滤器。

```c
/*
 * 函数作用：给 Master 配置“白名单”过滤器，只接收两个遥测 ID 和一个告警 ID。
 * 谁调用：Master 的 CanBasic_Init() 在 HAL_CAN_Start() 前调用一次。
 * 返回值：true 表示过滤器已写入硬件；false 时 CAN 不应继续启动。
 * 16 位列表模式有四个位置；本函数只用前三个位置。
 */
static bool CanBasic_ConfigMasterFilter(void)
{
    CAN_FilterTypeDef filter;

    memset(&filter, 0, sizeof(filter));
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;     /* 列表模式，不是掩码模式。 */
    filter.FilterScale = CAN_FILTERSCALE_16BIT;    /* 一个 bank 有四个列表位置。 */

    /* 标准 ID 在过滤器寄存器里必须左移 5 位。 */
    filter.FilterIdHigh = (uint16_t)(CAN_ID_S1_TELEMETRY << 5);
    filter.FilterIdLow = (uint16_t)(CAN_ID_S2_TELEMETRY << 5);
    filter.FilterMaskIdHigh = (uint16_t)(CAN_ID_S2_ALARM << 5);
    filter.FilterMaskIdLow = 0xFFFFU;              /* 第四个位置无效，不会匹配标准数据帧。 */

    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    return HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK;
}

/*
 * 函数作用：Slave 1 只接收 Master 发来的 0x100 查询命令。
 * 谁调用：Slave 1 的 CanBasic_Init()。
 * 32 位掩码模式适合“必须精确匹配一个 ID”的情况。
 */
static bool CanBasic_ConfigSlave1Filter(void)
{
    CAN_FilterTypeDef filter;

    memset(&filter, 0, sizeof(filter));
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;

    filter.FilterIdHigh = (uint16_t)(CAN_ID_S1_COMMAND << 5);
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = 0xFFE0U;  /* 比较标准 ID 的全部 11 位。 */
    filter.FilterMaskIdLow = 0x0006U;   /* 同时要求 IDE=0、RTR=0，即标准数据帧。 */

    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    return HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK;
}

/*
 * 函数作用：Slave 2 只接收 0x101 查询命令。
 * 与 Slave 1 的区别只有 command ID；其余过滤规则完全相同。
 */
static bool CanBasic_ConfigSlave2Filter(void)
{
    CAN_FilterTypeDef filter;

    memset(&filter, 0, sizeof(filter));
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = (uint16_t)(CAN_ID_S2_COMMAND << 5);
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = 0xFFE0U;
    filter.FilterMaskIdLow = 0x0006U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    return HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK;
}
```

在每个节点的 `CanBasic_Init()` 内，按节点角色调用其中一个过滤函数。先分别验证：

- Master 发 `0x100`，只有 Slave 1 的接收计数增加。
- Master 发 `0x101`，只有 Slave 2 的接收计数增加。
- 发 `0x555`，三个节点的应用接收计数均不增加。

---

# Day 6：教程 09，中断接收

教程 09 的核心只有两步：FIFO0 有消息触发中断；中断里用 `CAN_Receive()` 读出来。先照教程做一个最小版，再解释为什么要升级。

## 先理解中断实际经过哪些函数

教程中你自己写的是 `USB_LP_CAN1_RX0_IRQHandler()`；HAL 工程中这个函数仍然存在，但它只交给 HAL 处理。你真正写“取一帧”的代码放在 HAL 回调里。

```text
CAN 总线到来一帧
        |
        v
CAN 硬件把帧放入 FIFO0
        |
        v
USB_LP_CAN1_RX0_IRQHandler()        <- CubeMX 生成/保留
        |
        v
HAL_CAN_IRQHandler(&hcan)            <- CubeMX 生成/保留
        |
        v
HAL_CAN_RxFifo0MsgPendingCallback()  <- 你在 main.c 中写
        |
        v
CanBasic_RxFifo0Callback()           <- 你在 can_basic.c 中写
        |
        v
HAL_CAN_GetRxMessage()               <- 真正从 FIFO0 取走一帧
```

所以四个函数缺一不可。你只写 `HAL_CAN_RxFifo0MsgPendingCallback()` 但没有正确 IRQ 入口，中断不会进来；你只开 NVIC 但没有调用 `HAL_CAN_ActivateNotification()`，HAL 也不会通知 FIFO0 有新帧。

## 第 1 步：CubeMX 必须这样设置

在每一个节点的 CubeMX 中：

1. CAN1 的 Mode 保持 `Normal`（若还在 Day 2，可先用 Loopback）。
2. 打开 `NVIC Settings`。
3. 勾选 `USB low priority or CAN RX0 interrupts`，生成的名字是 `USB_LP_CAN1_RX0_IRQn`。
4. 当前 Day 6 会用 FreeRTOS 队列，因此抢占优先级设置为 **11**，子优先级 0。
5. 点击 Generate Code。

### 如果 CubeMX 只生成 `CAN1_RX1_IRQHandler`

你现在看到的：

```c
/*
 * 这是用户当前 CubeMX 生成的 FIFO1 中断入口，仅用于解释，不要把它复制到 FIFO0 方案。
 * CAN1_RX1_IRQHandler 对应 CAN1_RX1_IRQn；FIFO0 必须使用后文的 USB_LP_CAN1_RX0_IRQHandler。
 */
void CAN1_RX1_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}
```

说明你在 CubeMX 的 NVIC 中勾选的是 **CAN1 RX1 interrupt**，也就是接收 FIFO1；它不是 FIFO0 的中断入口。

本计划后面的代码统一使用 FIFO0（`CAN_RX_FIFO0`），因此推荐做法是：

1. 回到 CubeMX 的 `System Core -> NVIC`。
2. 取消勾选 `CAN1 RX1 interrupt`。
3. 勾选 `USB low priority or CAN RX0 interrupts`。
4. 生成代码。

这样才会得到 `USB_LP_CAN1_RX0_IRQHandler()`，并与后文的 `CAN_RX_FIFO0`、`CAN_IT_RX_FIFO0_MSG_PENDING`、`HAL_CAN_RxFifo0MsgPendingCallback()` 完全一致。

不要手工把 `CAN1_RX1_IRQHandler` 改名为 `USB_LP_CAN1_RX0_IRQHandler`。中断函数名由 STM32F103 的启动文件和 NVIC 向量表决定，改名不会把 FIFO1 的中断变成 FIFO0。

如果你暂时必须沿用 FIFO1，也可以全部改成 FIFO1，但初学阶段不推荐同时改两套名称。必须对应替换的地方如下：

| FIFO0 计划写法 | FIFO1 的对应写法 |
|---|---|
| `USB_LP_CAN1_RX0_IRQHandler` | `CAN1_RX1_IRQHandler` |
| `CAN_RX_FIFO0` | `CAN_RX_FIFO1` |
| `CAN_IT_RX_FIFO0_MSG_PENDING` | `CAN_IT_RX_FIFO1_MSG_PENDING` |
| `HAL_CAN_RxFifo0MsgPendingCallback` | `HAL_CAN_RxFifo1MsgPendingCallback` |

但是你的过滤器也必须把 `filter.FilterFIFOAssignment` 从 `CAN_FILTER_FIFO0` 改为 `CAN_FILTER_FIFO1`；否则帧仍会放进 FIFO0，FIFO1 永远没有中断。所以建议现在就回 CubeMX 改回 RX0，后面的所有代码不用变。

生成后，检查 `Core/Inc/stm32f1xx_it.h` 中有这一行：

```c
/* 这是 stm32f1xx_it.h 中的函数声明，不是函数实现。 */
void USB_LP_CAN1_RX0_IRQHandler(void);
```

并检查 `Core/Src/stm32f1xx_it.c` 中有下面函数。通常 CubeMX 已自动生成；如果没有，复制到该文件对应的 `USER CODE BEGIN` 区域外。不要在 `main.c` 再写一个同名 IRQ 函数。

```c
/*
 * 这是 STM32 真正的中断入口。
 * 函数名由芯片启动文件固定，不能随意改名。
 */
/*
 * 函数作用：STM32F103 的 FIFO0 硬件中断入口。
 * 谁调用：CAN1 FIFO0 有新消息时，NVIC 根据启动文件向量表自动跳转到这里。
 * 注意：名称必须完全固定；不要把它移动到 main.c，也不要手工改成 RX1 名称。
 */
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    /*
     * 不在这里读取数据、不打印、不延时。
     * HAL 会判断哪个 CAN 事件发生，然后调用下面的 HAL 回调函数。
     */
    HAL_CAN_IRQHandler(&hcan);
}
```

`stm32f1xx_it.c` 文件顶部需要能看到 `hcan` 的声明。最简单且正确的方式是在 includes 区加入 CubeMX 的头文件：

```c
/* USER CODE BEGIN Includes */
#include "can.h"
/* USER CODE END Includes */
```

## 第 2 步：在 `CanBasic_Init()` 中真正打开接收通知

仅配置 NVIC 还不够。下面是 Day 6 后 `CanBasic_Init()` 的完整版本；替换 Day 2 的同名函数。

```c
bool CanBasic_Init(void)
{
    /* Day 5 起，此处调用当前节点对应的过滤器函数。 */
    if (!CanBasic_ConfigAcceptAllFilter()) {
        return false;
    }

    /* 让 CAN 外设离开初始化状态，开始参与总线。 */
    if (HAL_CAN_Start(&hcan) != HAL_OK) {
        return false;
    }

    /*
     * 允许 HAL 在 FIFO0 有新消息时调用 RxFifo0MsgPendingCallback。
     * 没有这行，即使 IRQ 已启用，也不会进入我们写的接收回调。
     */
    if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
        return false;
    }

    return true;
}
```

> Day 5 已换精确过滤器时，把第一行的 `CanBasic_ConfigAcceptAllFilter()` 替换为本节点实际使用的 `CanBasic_ConfigMasterFilter()`、`CanBasic_ConfigSlave1Filter()` 或 `CanBasic_ConfigSlave2Filter()`；其余中断代码不变。

## 第 3 步：在 `main.c` 写 HAL 回调函数

这个回调不能放在 `stm32f1xx_it.c`，放在 `Core/Src/main.c` 的 `/* USER CODE BEGIN 4 */` 到 `/* USER CODE END 4 */` 中。先确认 `main.c` 顶部已经有：

```c
/* USER CODE BEGIN Includes */
#include "can_basic.h"
/* USER CODE END Includes */
```

然后加入完整函数：

```c
/*
 * HAL_CAN_IRQHandler() 检测到 FIFO0 有帧后，自动调用本函数。
 * 本函数必须只转交给自己的 CAN 接收函数，保持很短。
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_handle)
{
    CanBasic_RxFifo0Callback(can_handle);
}
```

不要自己主动调用 `HAL_CAN_RxFifo0MsgPendingCallback()`；它只能由 HAL 在中断处理过程中调用。

## 先写教程同款最小版

放在每个节点的 `Core/Src/can_basic.c` 顶部：

```c
/* 中断和主循环都会使用，所以必须 volatile。 */
static volatile uint8_t s_rx_flag;
static uint16_t s_rx_id;
static uint8_t s_rx_dlc;
static uint8_t s_rx_data[8];

/*
 * 函数作用：中断到来时，从硬件 FIFO0 取走一帧，保存到教程同款全局变量。
 * 谁调用：main.c 的 HAL_CAN_RxFifo0MsgPendingCallback() 调用；实际运行在中断上下文。
 * 参数：can_handle 是 HAL 传来的 CAN 句柄，正常情况下就是 &hcan。
 * 返回值：无。中断函数不能等待、不能 printf、不能 OLED 刷屏。
 * 局限：s_rx_data 只有一份，新帧可能覆盖旧帧；这一点正是后面引入队列的原因。
 */
void CanBasic_RxFifo0Callback(CAN_HandleTypeDef *can_handle)
{
    CAN_RxHeaderTypeDef header;

    /* 只处理本工程的 CAN1。 */
    if (can_handle != &hcan) {
        return;
    }

    /* 从 FIFO0 取一帧，和教程 CAN_Receive() 完全对应。 */
    if (HAL_CAN_GetRxMessage(can_handle, CAN_RX_FIFO0, &header, s_rx_data) != HAL_OK) {
        return;
    }

    /* 再做一次帧类型检查，即使 Day 5 的过滤器已限制标准数据帧。 */
    if ((header.IDE != CAN_ID_STD) || (header.RTR != CAN_RTR_DATA) || (header.DLC > 8U)) {
        return;
    }

    s_rx_id = (uint16_t)header.StdId;
    s_rx_dlc = header.DLC;
    s_rx_flag = 1U; /* 主循环看到 1 后才处理数据。 */
}

/*
 * 函数作用：让主循环/普通任务安全地拿走中断保存的一帧。
 * 谁调用：Day 6 的测试主循环；不要在中断回调中调用它。
 * 参数：std_id、data、dlc 都是输出参数，不能传 NULL。
 * 返回值：true 表示已经复制出一帧；false 表示中断还没有收帧。
 */
bool CanBasic_TakeReceived(uint16_t *std_id, uint8_t *data, uint8_t *dlc)
{
    if ((std_id == NULL) || (data == NULL) || (dlc == NULL) || (s_rx_flag == 0U)) {
        return false;
    }

    /* 先清标志，再复制数据；此版一次只能保存一帧。 */
    s_rx_flag = 0U;
    *std_id = s_rx_id;
    *dlc = s_rx_dlc;
    memcpy(data, s_rx_data, s_rx_dlc);
    return true;
}
```

`can_basic.h` 增加声明：

```c
void CanBasic_RxFifo0Callback(CAN_HandleTypeDef *hcan);
bool CanBasic_TakeReceived(uint16_t *std_id, uint8_t *data, uint8_t *dlc);
```

`main.c` 的 USER CODE 4 加入：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CanBasic_RxFifo0Callback(hcan);
}
```

并在 `CanBasic_Init()` 的 `HAL_CAN_Start()` 后加：

```c
if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
    return false;
}
```

上面两段已经在“第 2 步”和“第 3 步”给出了完整上下文；这里保留它们，是为了你在写 `CanBasic_Init()` 和 `main.c` 时方便回看。

## 第 4 步：怎样验证中断真的进来了

不要一上来相信中断已配置好。添加一个计数变量，收到一帧就加 1，主循环或任务每秒打印一次。

在 `can_basic.c` 顶部增加：

```c
static volatile uint32_t s_rx_irq_count;

/*
 * 函数作用：返回已成功接收的中断帧数，只用于 Day 6 验证。
 * 谁调用：普通任务或主循环；返回值不是协议数据，不参与业务逻辑。
 */
uint32_t CanBasic_GetRxIrqCount(void)
{
    return s_rx_irq_count;
}
```

在 `CanBasic_RxFifo0Callback()` 的 `HAL_CAN_GetRxMessage()` 成功、并且帧检查通过后，加入：

```c
s_rx_irq_count++; /* 每成功处理一帧，计数加一。 */
```

在 `can_basic.h` 增加声明：

```c
uint32_t CanBasic_GetRxIrqCount(void);
```

最后在普通任务或 `while(1)` 中每秒打印：

```c
Serial_Printf("CAN IRQ count = %lu\r\n", (unsigned long)CanBasic_GetRxIrqCount());
HAL_Delay(1000U);
```

当另一节点每 500 ms 发一帧时，这个计数大约每秒增加 2。若始终为 0，按下面顺序检查：

1. 对方是否真的发出了帧（先用 Day 3 的轮询版本交叉确认）。
2. 本节点是否已调用 `CanBasic_Init()`。
3. `HAL_CAN_ActivateNotification()` 是否返回 `HAL_OK`。
4. `USB_LP_CAN1_RX0_IRQHandler()` 是否存在且调用 `HAL_CAN_IRQHandler(&hcan)`。
5. CubeMX 是否勾选 `USB_LP_CAN1_RX0_IRQn`。
6. Day 5 的过滤器是否错误拦掉了帧；排查时可暂时换回全接收过滤器。

## 这版为什么不能直接作为最终版

它和教程完全一致，但 `s_rx_data` 只有一个。若 Slave 1 的一帧尚未被任务拿走，Slave 2 又到一帧，第一帧可能被覆盖。现在你已经理解“中断收一帧”的动作，下一小步才换成队列。

## 改成 FreeRTOS 队列：只替换接收保存位置

新建 `Core/Inc/can_rx_queue.h`：

```c
#ifndef CAN_RX_QUEUE_H
#define CAN_RX_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "can.h"

typedef struct {
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];
} CanRxFrame_t;

bool CanRxQueue_Init(void);
void CanRxQueue_PutFromIsr(CAN_HandleTypeDef *hcan);
bool CanRxQueue_Get(CanRxFrame_t *frame, TickType_t wait_ticks);
uint32_t CanRxQueue_GetDropCount(void);

#endif
```

新建 `Core/Src/can_rx_queue.c`：

```c
#include "can_rx_queue.h"

#include <string.h>

static QueueHandle_t s_rx_queue;
static uint32_t s_rx_drop_count;
static volatile uint32_t s_rx_irq_count;

/*
 * 函数作用：创建接收队列，替代教程版的一个 s_rx_data 数组。
 * 谁调用：启动 FreeRTOS 调度器之前调用一次。
 * 返回值：内存足够并创建成功返回 true；NULL 表示 FreeRTOS 堆不足。
 */
bool CanRxQueue_Init(void)
{
    /* 队列一次保存 16 帧；每帧都是 ID、DLC、8 字节数据。 */
    s_rx_queue = xQueueCreate(16U, sizeof(CanRxFrame_t));

    /* 返回 false 说明 FreeRTOS 堆不够，调用者必须停止启动，而不是继续运行。 */
    return s_rx_queue != NULL;
}

/*
 * 函数作用：在 CAN RX0 中断中读取一帧，并把完整帧放入 FreeRTOS 队列。
 * 谁调用：HAL_CAN_RxFifo0MsgPendingCallback()；运行在中断上下文。
 * 参数：hcan 是发生中断的 CAN 句柄。
 * 返回值：无；队列满时不能等待，只累计丢帧数。
 */
void CanRxQueue_PutFromIsr(CAN_HandleTypeDef *hcan)
{
    CAN_RxHeaderTypeDef header;
    CanRxFrame_t frame;
    /* task_woken 用来告诉 FreeRTOS：是否有高优先级任务应立刻运行。 */
    BaseType_t task_woken = pdFALSE;

    memset(&frame, 0, sizeof(frame));
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, frame.data) != HAL_OK) {
        return;
    }
    if ((header.IDE != CAN_ID_STD) || (header.RTR != CAN_RTR_DATA) || (header.DLC > 8U)) {
        return;
    }

    frame.id = (uint16_t)header.StdId;
    frame.dlc = header.DLC;
    s_rx_irq_count++; /* 每成功从 FIFO0 读出一帧，记录一次中断接收。 */

    /* 与教程不同的唯一一行：把帧放进队列，而不是覆盖一个全局变量。 */
    if (xQueueSendFromISR(s_rx_queue, &frame, &task_woken) != pdPASS) {
        s_rx_drop_count++;
    }
    portYIELD_FROM_ISR(task_woken);
}

/*
 * 函数作用：普通任务从 RX 队列取一帧。
 * 谁调用：Master 的 CanTask 或 Slave 的命令接收任务。
 * wait_ticks：portMAX_DELAY 表示没有帧时一直阻塞，不占 CPU。
 * 返回值：取到一帧返回 true；等待超时或参数错误返回 false。
 */
bool CanRxQueue_Get(CanRxFrame_t *frame, TickType_t wait_ticks)
{
    /* xQueueReceive 成功会把队首的一帧复制到 *frame，并自动从队列删除该帧。 */
    return xQueueReceive(s_rx_queue, frame, wait_ticks) == pdPASS;
}

/* 函数作用：读取“队列满导致丢帧”的次数，用于 Day 13~14 通信诊断。 */
uint32_t CanRxQueue_GetDropCount(void)
{
    /* 这里只读计数，不清零；因此可用于观察丢帧是否仍在持续增加。 */
    return s_rx_drop_count;
}

/* 函数作用：读取 FIFO0 中断成功取帧的次数，用于确认中断真的在工作。 */
uint32_t CanRxQueue_GetIrqCount(void)
{
    /* 同样只读不清零，便于每秒打印时看到累计值。 */
    return s_rx_irq_count;
}
```

同时在 `can_rx_queue.h` 增加：

```c
uint32_t CanRxQueue_GetIrqCount(void);
```

换成队列版后，用 `CanRxQueue_GetIrqCount()` 验证中断；不要继续读取教程版的 `CanBasic_GetRxIrqCount()`。

最终 `HAL_CAN_RxFifo0MsgPendingCallback()` 改成：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CanRxQueue_PutFromIsr(hcan);
}
```

## 队列版到底放在哪里、哪些旧代码必须删除

这一步很重要：**教程单变量版和 FreeRTOS 队列版只能二选一**。两个版本都调用 `HAL_CAN_GetRxMessage()`；如果同时保留，先执行的函数已经把 FIFO0 中的帧取走，另一个函数就收不到。

按下面顺序替换：

1. 把 `can_rx_queue.h/.c` 加入 Keil 工程。
2. 在每个节点启动调度器之前调用一次 `CanRxQueue_Init()`。
3. 成功后才调用 `CanBasic_Init()`，因为 `CanBasic_Init()` 会打开 FIFO0 接收中断。
4. 删除或注释 Day 6 教程版的 `CanBasic_RxFifo0Callback()`、`CanBasic_TakeReceived()` 和 `s_rx_flag/s_rx_id/s_rx_dlc/s_rx_data`。
5. `main.c` 中只保留队列版的 `HAL_CAN_RxFifo0MsgPendingCallback()`。

最小初始化位置如下。直接放在各节点 `main.c` 调用 `sensor_start()` 之前；它必须在 `vTaskStartScheduler()` 之前执行：

```c
/*
 * 函数作用：先创建软件 RX 队列，再启动 CAN 硬件。
 * 为什么这个顺序不能反：CAN 一旦启动就可能进中断；队列还没创建时无法保存收到的帧。
 */
if (!CanRxQueue_Init()) {
    Error_Handler(); /* FreeRTOS 堆不足，不能继续。 */
}

if (!CanBasic_Init()) {
    Error_Handler(); /* 过滤器、CAN 启动或中断通知失败。 */
}
```

`HAL_CAN_RxFifo0MsgPendingCallback()` 在整个工程里也只能定义一次。建议统一放在 `Core/Src/main.c` 的 `/* USER CODE BEGIN 4 */` 中：

```c
/*
 * 函数作用：HAL 的固定回调入口；FIFO0 有帧时由 HAL 自动调用。
 * 谁调用：USB_LP_CAN1_RX0_IRQHandler() -> HAL_CAN_IRQHandler()。
 * 这里不读取业务协议，只把取帧工作交给队列函数。
 */
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *hcan)
{
    CanRxQueue_PutFromIsr(hcan);
}
```

## 验收

- [x] 先跑通教程同款单变量中断版。
- [ ] 再换队列版；连续快速发送时，`CanRxQueue_GetDropCount()` 为 0。
- [ ] ISR 里没有 `printf`、OLED、`HAL_Delay`。

---

# Day 7：教程 10，定时、按键、查询发送

教程 10 的三种方式是：定时发送、按键触发、收到请求发送。FreeRTOS 的对应关系很简单：

| 教程 | 本项目 |
|---|---|
| 定时标志 | `vTaskDelayUntil()` |
| 主循环按键 | 按键中断唤醒 `ButtonTask` |
| 收到请求后发送 | 从节点命令接收任务通知现有的 `Slave1CanTask` / `Slave2CanTask` |

## 先建立最简单的周期发送任务

```c
/*
 * 函数作用：每 500 ms 发送一帧测试数据，验证“定时发送”。
 * 谁调用：FreeRTOS 调度器创建后自动运行，不需要手工调用。
 * argument：FreeRTOS 固定任务参数；本例不传任何参数，所以用 (void)argument 消除未使用警告。
 */
static void PeriodicCanTask(void *argument)
{
    uint8_t data[8] = {0U}; /* 8 字节 CAN 数据区；初始化为 0 防止发送垃圾数据。 */
    TickType_t last_wake = xTaskGetTickCount(); /* 记录本周期基准时刻。 */
    (void)argument;

    while (1) {
        data[0]++; /* 每 500 ms 改变一次，便于确认确实在周期发送。 */
        (void)CanBasic_Send(0x180U, data, 8U);

        /* 教程的定时器标志换成 RTOS 的精确定时等待。 */
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
    }
}
```

创建任务：

```c
/*
 * 函数作用：创建 Day 7 的周期发送任务。
 * 谁调用：启动调度器之前的应用初始化函数中调用一次。
 * 192U：任务栈深度，单位是 word，不是 byte；后续可用栈水位再调整。
 * 2U：任务优先级；高于 OLED 之类显示任务，低于 CAN 接收任务。
 */
if (xTaskCreate(PeriodicCanTask, "CanPeriod", 192U, NULL, 2U, NULL) != pdPASS) {
    Error_Handler();
}
```

今天 `CanBasic_Send()` 可以继续由业务任务直接调用，因为你先理解“什么时候发送”。本项目发送任务数量很少，后续仍由业务任务直接发送；暂时不增加 TX 队列，避免没有实际收益的复杂化。

## 验收

- [ ] Master 每秒收到约两帧 `0x180`。
- [ ] 改变发送数据的计数每帧递增。
- [ ] 任务等待时，其他 FreeRTOS 任务继续运行。

---

## Day 8~10 先确定传感器分配

前面的写法容易把“ADC”和“传感器”混为一谈。**ADC 是 STM32 内部的模拟量采集外设，不是一种传感器。**
光敏电阻、热敏电阻输出的是模拟电压，才需要接到 ADC；BH1750 和 DS18B20 则分别使用 I2C、1-Wire，
不经过 ADC。

这套三节点工程建议按下面方式接线：

| 节点 | 实际传感器 | 接口 | 在代码中的任务 |
|---|---|---|---|
| Master | 不接采集传感器 | CAN、OLED、按键、状态 LED | `CanTask` 接收并显示从节点数据 |
| Slave 1 | 光敏电阻模块 | ADC1_IN0，即 PA0 | `AdcTask` |
| Slave 1 | BH1750 | I2C（PB8/PB9） | `LightTask` |
| Slave 2 | 热敏电阻模块 | ADC1_IN0，即 PA0 | `AdcTask` |
| Slave 2 | DS18B20 | 1-Wire GPIO | `TempTask` |

所以 Day 8 和 Day 9 都出现 `AdcTask` 是有意的：两个从节点各自读取本地不同的模拟传感器，
并不是重复读取同一个传感器。Master 没有传感器也完全合理，它的职责是接收、汇总、显示和发出查询命令。

当前三个工程的 `adc.c` 都预先配置了 PA0、PA1 两个 ADC 通道，但现有 `AdcTask` 只使用 `Get_AD_Val()[0]`，
也就是 PA0。因此本计划统一把模拟传感器接 PA0；PA1 是后续扩展通道，暂时不要接线或在协议中使用。
如果以后想让一块从节点同时接光敏电阻和热敏电阻，就要让任务同时读取 `ad[0]`、`ad[1]`，并给快照和 CAN 帧增加第二个模拟量字段。

套件中的红外模块、旋转编码器、MPU6050、电机、舵机、Flash 等先不放进 Day 8~10；它们会同时引入新的 GPIO、I2C、PWM 或存储问题，
不适合作为本阶段 CAN 数据链路的必需项。

---

# Day 8：Slave 1 用现有任务发送 ADC 和光照

## 今天只完成一条链路

```text
AdcTask / LightTask
        -> sensorQueue
        -> DataTask
        -> g_snapshot
        -> Slave1CanTask
        -> CanBasic_Send(0x180)
```

你已经写好了前四步，所以今天不新建 `slave1_can.c`，也不在 CAN 任务里重新读取 ADC、BH1750。
只在现有的 `slave_node1/Core/Src/app_sensor.c` 中增加一个发送任务。

这里的 ADC 指 **Slave 1 上 PA0 接的光敏电阻模拟电压**，BH1750 是另一种数字光照测量，
两者放在同一个从节点上，正好可以比较“模拟量原始值”和“已经换算成 lux 的数字值”。

这样分工很明确：

- `AdcTask` 只采 ADC。
- `LightTask` 只采光照。
- `DataTask` 只更新 `g_snapshot`。
- `Slave1CanTask` 只复制快照、组帧、发送。

## 先确定 8 字节格式

Slave 1 使用标准 ID `0x180`，DLC 固定为 8：

| 字节 | 内容 | 例子 |
|---|---|---|
| Byte 0 | 协议版本，固定为 `1` | `01` |
| Byte 1 | 发送序号，每帧加一 | `00, 01, 02...` |
| Byte 2~3 | ADC，16 位无符号数，高字节在前 | ADC=`0x1234` 时发送 `12 34` |
| Byte 4~5 | 光照整数 lux，高字节在前 | 300 lux 发送 `01 2C` |
| Byte 6 bit0 | ADC 是否有效 | 有效为 1 |
| Byte 6 bit1 | 光照是否有效 | 有效为 1 |
| Byte 7 | 预留，今天固定为 0 | `00` |

Byte 6 的常用值只有三个：`0x00` 都无效、`0x01` 只有 ADC 有效、`0x03` 两者都有效。

## 第一步：在 `app_sensor.c` 顶部增加声明

先增加头文件：

```c
#include "can_basic.h"
#include "can_protocol.h"
```

再放到其他任务声明旁边：

```c
#define Slave1CanTask_STACKDEPTH 256
#define Slave1CanTask_PRIORITY   2
TaskHandle_t Slave1CanTask_Handle;
void Slave1CanTask(void *pvParameters);
```

## 第二步：在 `task_start()` 中创建正确的四个任务

Slave 1 今天需要运行：`AdcTask`、`LightTask`、`DataTask`、`Slave1CanTask`。
`TempTask` 属于 Slave 2，先不要创建；`UITask` 只用于从节点本地观察，可暂时保留，也可先关闭。

在你现有 `task_start()` 中增加：

```c
xTaskCreate((TaskFunction_t)Slave1CanTask,
            (char *)"Slave1CanTask",
            (configSTACK_DEPTH_TYPE)Slave1CanTask_STACKDEPTH,
            (void *)NULL,
            (UBaseType_t)Slave1CanTask_PRIORITY,
            (TaskHandle_t *)&Slave1CanTask_Handle);
```

不要再写第二个 `vTaskStartScheduler()`。你现有的 `sensor_start()` 已经负责启动调度器。

## 第三步：在 `app_sensor.c` 末尾增加发送任务

```c
void Slave1CanTask(void *pvParameters)
{
    SensorSnapshot_t local;
    uint8_t data[8] = {0};
    uint8_t sequence = 0U;
    TickType_t last_wake = xTaskGetTickCount();

    (void)pvParameters;

    while (1) {
        /* 只在复制快照时持有互斥锁，CAN 发送时不占锁。 */
        xSemaphoreTake(snapshotMutex, portMAX_DELAY);
        local = g_snapshot;
        xSemaphoreGive(snapshotMutex);

        data[0] = 1U;
        data[1] = sequence++;
        CanProtocol_PutU16BE(&data[2], local.adc);
        CanProtocol_PutU16BE(&data[4], (uint16_t)local.light_lux);

        data[6] = 0U;
        if (local.adc_valid != 0U) {
            data[6] |= 0x01U;
        }
        if (local.light_valid != 0U) {
            data[6] |= 0x02U;
        }
        data[7] = 0U;

        (void)CanBasic_Send(CAN_ID_S1_TELEMETRY, data, 8U);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
    }
}
```

这里不直接调用 `Get_AD_Val()` 或 `BH1750_ReadLight()`，因为现有采集任务已经在做这件事。
如果 CAN 任务再读一次传感器，就会出现两个任务同时管理同一硬件，代码反而更难理解。

## 第四步：修改 Slave 1 的 `main.c`

删掉原来裸机 `while(1)` 中发送 `{0x11, 0x22, 0x33, 0x44}` 的测试代码。初始化区只保留：

```c
if (!CanBasic_Init()) {
    Error_Handler();
}

sensor_start();
```

`sensor_start()` 启动调度器后不会正常返回，所以后面的裸机 `while(1)` 不再负责传感器或 CAN。

## 今天怎样验证

先让 Master 继续使用当前的原始帧显示任务，不急着解析：

- [ ] Master 收到 ID `0x180`，DLC 为 8。
- [ ] Byte 1 每 500 ms 增加一次。
- [ ] 遮挡或照亮 Slave 1 的光敏电阻模块时 Byte 2~3 改变。
- [ ] 遮住或照亮 BH1750 时 Byte 4~5 改变。
- [ ] Byte 6 正常时为 `0x03`。

只要这五项通过，Day 8 就结束；今天不做 Slave 2，也不做 Master 最终界面。

---

# Day 9：Slave 2 用现有任务发送 ADC 和温度

Day 9 与 Day 8 的结构完全相同，只替换两个东西：

- `LightTask` 换成 `TempTask`。
- CAN ID 从 `0x180` 换成 `0x181`。

这里的 ADC 指 **Slave 2 上 PA0 接的热敏电阻模拟电压**，温度字段来自 DS18B20。
热敏电阻和 DS18B20 都能反映温度，但前者是 ADC 原始量，后者是已经完成数字转换的温度，
放在同一从节点上可以同时练习两种采集接口，并在 Master 端比较两路结果。

数据流是：

```text
AdcTask / TempTask
        -> sensorQueue
        -> DataTask
        -> g_snapshot
        -> Slave2CanTask
        -> CanBasic_Send(0x181)
```

DS18B20 转换需要约 750 ms，这件事继续由你已经写好的 `TempTask` 负责。CAN 任务不能再次调用
`DS18B20_StartConversion()`，它只读取最近一次完成的温度快照。

## Slave 2 的帧格式

Slave 2 同样固定发送 8 字节：

| 字节 | 内容 | 例子 |
|---|---|---|
| Byte 0 | 协议版本，固定为 `1` | `01` |
| Byte 1 | 发送序号 | `00, 01, 02...` |
| Byte 2~3 | ADC，高字节在前 | ADC=`0x1234` 时发送 `12 34` |
| Byte 4~5 | 温度乘 10 后的有符号整数 | `25.3 C` 发送 `253`，即 `00 FD` |
| Byte 6 bit0 | ADC 是否有效 | 有效为 1 |
| Byte 6 bit1 | 温度是否有效 | 有效为 1 |
| Byte 7 | 预留，今天固定为 0 | `00` |

温度不直接发送 `float`。例如 `25.3` 转成 `253`，`-4.5` 转成 `-45`，Master 收到后再按
“除以 10”的含义显示。DS18B20 的范围远小于 `int16_t` 的范围，这里不需要复杂的溢出处理。

## 第一步：在 Slave 2 的 `app_sensor.c` 增加声明

增加头文件：

```c
#include "can_basic.h"
#include "can_protocol.h"
```

增加任务声明：

```c
#define Slave2CanTask_STACKDEPTH 256
#define Slave2CanTask_PRIORITY   2
TaskHandle_t Slave2CanTask_Handle;
void Slave2CanTask(void *pvParameters);
```

## 第二步：在 `task_start()` 中创建正确的四个任务

Slave 2 今天只需要：`AdcTask`、`TempTask`、`DataTask`、`Slave2CanTask`。
不要创建 `LightTask`。

```c
xTaskCreate((TaskFunction_t)Slave2CanTask,
            (char *)"Slave2CanTask",
            (configSTACK_DEPTH_TYPE)Slave2CanTask_STACKDEPTH,
            (void *)NULL,
            (UBaseType_t)Slave2CanTask_PRIORITY,
            (TaskHandle_t *)&Slave2CanTask_Handle);
```

## 第三步：增加发送任务

```c
void Slave2CanTask(void *pvParameters)
{
    SensorSnapshot_t local;
    uint8_t data[8] = {0};
    uint8_t sequence = 0U;
    int16_t temperature_x10;
    TickType_t last_wake = xTaskGetTickCount();

    (void)pvParameters;

    while (1) {
        xSemaphoreTake(snapshotMutex, portMAX_DELAY);
        local = g_snapshot;
        xSemaphoreGive(snapshotMutex);

        data[0] = 1U;
        data[1] = sequence++;
        CanProtocol_PutU16BE(&data[2], local.adc);

        /* 25.3 -> 253；-4.5 -> -45。 */
        temperature_x10 = (int16_t)(local.temp * 10.0f);
        CanProtocol_PutS16BE(&data[4], temperature_x10);

        data[6] = 0U;
        if (local.adc_valid != 0U) {
            data[6] |= 0x01U;
        }
        if (local.temp_valid != 0U) {
            data[6] |= 0x02U;
        }
        data[7] = 0U;

        (void)CanBasic_Send(CAN_ID_S2_TELEMETRY, data, 8U);
        vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500U));
    }
}
```

`TempTask` 每约 1 秒产生一个新温度，而 `Slave2CanTask` 每 500 ms 发送一次，所以连续两帧温度相同是正常的，
不是任务没有运行。Byte 1 仍会每帧递增。

## 第四步：修改 Slave 2 的 `main.c`

删除裸机轮询接收测试，初始化区改为：

```c
if (!CanBasic_Init()) {
    Error_Handler();
}

sensor_start();
```

## 今天怎样验证

Master 仍先显示原始帧：

- [ ] 收到 ID `0x181`，DLC 为 8。
- [ ] Byte 1 每 500 ms 增加。
- [ ] 捏住或加热 Slave 2 的热敏电阻模块时 Byte 2~3 改变。
- [ ] 改变 DS18B20 温度时 Byte 4~5 改变。
- [ ] 传感器正常时 Byte 6 为 `0x03`。
- [ ] Slave 1 的 `0x180` 和 Slave 2 的 `0x181` 都能持续收到。

今天仍不修改 Master 的最终显示逻辑；先确认两个发送端各自正确。

---

# Day 10：Master 在现有 `CanTask` 中解析并显示

Day 10 不新建 `master_can.c`，直接修改你已经写好的
`master_node/Core/Src/app_sensor.c` 中的 `CanTask()`。

你当前的 `CanTask()` 已经能完成：

```text
CanRxQueue_Get() -> 显示原始 ID、DLC、data[]
```

今天只在中间加入“根据 ID 解码”这一步：

```text
CAN RX0 中断
  -> CanRxQueue_PutFromIsr()
  -> CanTask 从队列取帧
  -> 根据 0x180 / 0x181 解码
  -> OLED 同时显示两个从节点的最新值
```

不要在 `HAL_CAN_RxFifo0MsgPendingCallback()` 中解析或操作 OLED。中断只负责把完整帧放进队列。

## 第一步：在 Master 的 `app_sensor.c` 增加协议头文件

```c
#include "can_protocol.h"
```

`app_sensor.h` 已经包含 `can_rx_queue.h`，不用重复创建另一套接收队列。

## 第二步：把 CAN 任务栈改为 256

你当前的 `CanTask` 会调用多层 OLED 函数，`128` 个 word 偏紧，先改为：

```c
#define CanTask_STACKDEPTH 256
#define CanTask_PRIORITY   3
```

这里的 256 是 word，在 STM32F103 上约为 1024 字节。

## 第三步：先看懂 Master 保存什么

在 `CanTask()` 前面增加一个结构体。它只保存两个从节点最近一次收到的数据：

```c
typedef struct {
    uint16_t s1_adc;
    uint16_t s1_light;
    uint16_t s2_adc;
    int16_t  s2_temp_x10;
    uint8_t  s1_status;
    uint8_t  s2_status;
} MasterCanData_t;
```

这里不需要互斥锁，因为只有 `CanTask` 一个任务读写这块局部变量。

## 第四步：用下面代码替换现有 `CanTask()`

```c
void CanTask(void *pvParameters)
{
    CanRxFrame_t frame;
    MasterCanData_t latest = {0};

    (void)pvParameters;

    while (1) {
        /* 没有帧时阻塞，让出 CPU；收到帧后由中断唤醒。 */
        if (!CanRxQueue_Get(&frame, portMAX_DELAY)) {
            continue;
        }

        /* 两种遥测帧都必须是版本 1、DLC 8。 */
        if ((frame.dlc != 8U) || (frame.data[0] != 1U)) {
            continue;
        }

        if (frame.id == CAN_ID_S1_TELEMETRY) {
            latest.s1_adc = CanProtocol_GetU16BE(&frame.data[2]);
            latest.s1_light = CanProtocol_GetU16BE(&frame.data[4]);
            latest.s1_status = frame.data[6];
        } else if (frame.id == CAN_ID_S2_TELEMETRY) {
            latest.s2_adc = CanProtocol_GetU16BE(&frame.data[2]);
            latest.s2_temp_x10 = CanProtocol_GetS16BE(&frame.data[4]);
            latest.s2_status = frame.data[6];
        } else {
            continue;
        }

        /* 每收到一帧就刷新一次，先追求逻辑直观。 */
        OLED_ShowString(1, 1, "S1AD:");
        OLED_ShowNum(1, 7, latest.s1_adc, 4);

        OLED_ShowString(2, 1, "LIGHT:");
        OLED_ShowNum(2, 7, latest.s1_light, 5);

        OLED_ShowString(3, 1, "S2AD:");
        OLED_ShowNum(3, 7, latest.s2_adc, 4);

        OLED_ShowString(4, 1, "T*10:");
        OLED_ShowSignedNum(4, 7, latest.s2_temp_x10, 4);
    }
}
```

Day 10 先显示 `T*10:253`，它表示 `25.3 C`。先确认 CAN 中传输的有符号整数正确，
暂时不增加小数点格式化代码。

`s1_status` 和 `s2_status` 今天先保存但不显示；Day 12 做掉线和传感器异常时再使用。

## 第五步：核对 Master 的启动顺序

Master 的 `main.c` 初始化区必须保持这个顺序：

```c
/* 1. 先创建队列，CAN 中断回调才有地方放数据。 */
if (!CanRxQueue_Init()) {
    Error_Handler();
}

/* 2. 再配置过滤器、启动 CAN、打开 FIFO0 通知。 */
if (!CanBasic_Init()) {
    Error_Handler();
}

/* 3. sensor_start() 创建 CanTask 并启动调度器。 */
sensor_start();
```

CAN 回调保持：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_handle)
{
    CanRxQueue_PutFromIsr(can_handle);
}
```

`CanRxQueue_Get()` 只能在调度器启动后的 `CanTask` 中调用，不能再放回 `main()` 的裸机
`while(1)`；否则 FreeRTOS 临界区会屏蔽优先级 11 的 CAN RX0 中断。

## 今天怎样验证

- [x] OLED 第 1、2 行显示 Slave 1 的 ADC 和光照。
- [x] OLED 第 3、4 行显示 Slave 2 的 ADC 和温度乘 10。
- [x] 只关闭 Slave 1 时，Slave 2 的数值仍继续更新。
- [x] 只关闭 Slave 2 时，Slave 1 的数值仍继续更新。
- [x] 同时运行 10 分钟，CAN 接收任务没有卡死，OLED 持续变化。

至此 Day 8~10 的职责很简单：Day 8 只负责 `0x180`，Day 9 只负责 `0x181`，
Day 10 只负责接收、区分 ID、解码和显示。

---

# Day 11：周期上报之外，按键请求立即刷新

## 先说明这个功能为什么存在

Day 8、Day 9 使用 500 ms 周期，是为了联调时很快看见数据变化。如果一直保持 500 ms 上报，按键查询最多只提前半秒，
实际意义确实很小。Day 11 在通信链路已经验证后，把两个从节点的常规上报周期改为 **2 秒**：

- 平时：每 2 秒自动发送一次，Master 不按键也能持续监测和判断掉线。
- 按键：PB1 请求 Slave 1、PB11 请求 Slave 2 立即补发一帧，不必等待下一个 2 秒周期。
- 查询回复发送的是“当前最新快照”，不会重新阻塞等待一次 BH1750 或 DS18B20 转换。

所以这里不是在“周期上报”和“查询上报”之间二选一，而是：

```text
2 秒周期上报 = 常态监测、在线检测
按键立即上报 = 用户主动刷新、练习 CAN 请求/回复
```

## 先固定命令和回复格式

Master 当前有两个按键，分别查询两个从节点；两个方向各自维护独立命令序号：

| 按键 | 方向 | CAN ID | Byte 0 | Byte 1 | Byte 2 |
|---|---|---:|---:|---:|---:|
| PB1 | Master -> Slave 1 | `0x100` | 版本 `1` | S1 命令序号 | `0x01` 立即上报 |
| PB11 | Master -> Slave 2 | `0x101` | 版本 `1` | S2 命令序号 | `0x01` 立即上报 |

Day 8、Day 9 中遥测帧的 Byte 7 原来是预留字节，今天正式定义：

| 遥测字段 | 周期上报 | 查询回复 |
|---|---:|---:|
| Byte 6 bit7 | `0` | `1` |
| Byte 7 | `0` | 回显收到的命令序号 |

这样 Master 能证明收到的确实是某次按键对应的回复，而不只是碰巧赶上的周期帧。

## 第一步：Master 用现有 `app_sensor.c` 发送命令

当前工程已经用 `KEY_INPUT()` 轮询 PB1、PB11，不增加按键中断。PB1 对应 Slave 1，PB11 对应 Slave 2，
两个引脚都配置为上拉输入，按键另一端接 GND。

在 `master_node/Core/Src/app_sensor.c` 顶部增加：

```c
#include "can_basic.h"

#define ButtonTask_STACKDEPTH 128
#define ButtonTask_PRIORITY   2
TaskHandle_t ButtonTask_Handle;
void ButtonTask(void *pvParameters);
```

在 Master 的 `task_start()` 中，保留现有 `CanTask`，再创建一个 `ButtonTask`：

```c
xTaskCreate((TaskFunction_t)ButtonTask,
            (char *)"ButtonTask",
            (configSTACK_DEPTH_TYPE)ButtonTask_STACKDEPTH,
            (void *)NULL,
            (UBaseType_t)ButtonTask_PRIORITY,
            (TaskHandle_t *)&ButtonTask_Handle);
```

把完整任务函数放在 `app_sensor.c` 末尾：

```c
void ButtonTask(void *pvParameters)
{
    uint8_t command[8] = {1U, 0U, CAN_COMMAND_QUERY_NOW, 0U, 0U, 0U, 0U, 0U};
    uint8_t command_sequence_s1 = 0U;
    uint8_t command_sequence_s2 = 0U;
    uint8_t key;

    (void)pvParameters;

    while (1) {
        key = KEY_INPUT();
        if (key == 1U) {
            command[1] = command_sequence_s1++;
            (void)CanBasic_Send(CAN_ID_S1_COMMAND, command, 8U);
        } else if (key == 2U) {
            command[1] = command_sequence_s2++;
            (void)CanBasic_Send(CAN_ID_S2_COMMAND, command, 8U);
        }
        vTaskDelay(pdMS_TO_TICKS(10U));
    }
}
```

## 第二步：给两个从节点补上命令接收队列

当前 Slave 1、Slave 2 只有发送任务，没有 `can_rx_queue.c/.h`，所以它们无法接收 `0x100/0x101`。
把 Master 已经验证过的下面两个文件各复制一份到两个从节点，并加入各自的 Keil 工程：

```text
master_node/Core/Inc/can_rx_queue.h -> slave_node1/Core/Inc/ 和 slave_node2/Core/Inc/
master_node/Core/Src/can_rx_queue.c -> slave_node1/Core/Src/ 和 slave_node2/Core/Src/
```

在两个从节点的 `app_sensor.h` 中增加：

```c
#include "can_rx_queue.h"
```

两个从节点的 `CanBasic_Init()` 在 `HAL_CAN_Start()` 成功后还要打开 FIFO0 通知：

```c
if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
    return false;
}
```

还要在两个从节点的 CubeMX 中打开 `CAN1 RX0 interrupt`，抢占优先级设为 11。当前两个从节点的
`can.c` 只配置了 PA11/PA12，并没有启用这个 NVIC 中断。重新生成后应看到：

```c
HAL_NVIC_SetPriority(USB_LP_CAN1_RX0_IRQn, 11, 0);
HAL_NVIC_EnableIRQ(USB_LP_CAN1_RX0_IRQn);
```

`stm32f1xx_it.c` 中也必须存在下面的 IRQ 入口；如果 CubeMX 已经生成，不要再写第二份：

```c
void USB_LP_CAN1_RX0_IRQHandler(void)
{
    HAL_CAN_IRQHandler(&hcan);
}
```

两个从节点的 `main.c` 都必须保持这个顺序，不能把 `sensor_start()` 放到 CAN 初始化前面：

```c
if (!CanRxQueue_Init()) {
    Error_Handler();
}

if (!CanBasic_Init()) {
    Error_Handler();
}

sensor_start();
```

并在两个从节点 `main.c` 的 `/* USER CODE BEGIN 4 */` 中增加同一个回调：

```c
void HAL_CAN_RxFifo0MsgPendingCallback(CAN_HandleTypeDef *can_handle)
{
    CanRxQueue_PutFromIsr(can_handle);
}
```

## 第三步：Slave 1 收到 `0x100` 后唤醒现有发送任务

在 `slave_node1/Core/Src/app_sensor.c` 增加声明：

```c
#define Slave1CommandTask_STACKDEPTH 192
#define Slave1CommandTask_PRIORITY   3
TaskHandle_t Slave1CommandTask_Handle;
void Slave1CommandTask(void *pvParameters);
```

在 `task_start()` 中先创建现有 `Slave1CanTask`，再创建：

```c
xTaskCreate((TaskFunction_t)Slave1CommandTask,
            (char *)"S1Command",
            (configSTACK_DEPTH_TYPE)Slave1CommandTask_STACKDEPTH,
            (void *)NULL,
            (UBaseType_t)Slave1CommandTask_PRIORITY,
            (TaskHandle_t *)&Slave1CommandTask_Handle);
```

命令任务只负责检查协议，并把命令序号通过任务通知交给发送任务：

```c
void Slave1CommandTask(void *pvParameters)
{
    CanRxFrame_t frame;

    (void)pvParameters;

    while (1) {
        if (!CanRxQueue_Get(&frame, portMAX_DELAY)) {
            continue;
        }

        if ((frame.id == CAN_ID_S1_COMMAND) &&
            (frame.dlc == 8U) &&
            (frame.data[0] == 1U) &&
            (frame.data[2] == CAN_COMMAND_QUERY_NOW)) {
            xTaskNotify(Slave1CanTask_Handle,
                        (uint32_t)frame.data[1],
                        eSetValueWithOverwrite);
        }
    }
}
```

然后修改 `Slave1CanTask()`：删掉原来的 `last_wake` 和末尾 `vTaskDelayUntil()`，在循环最前面等待通知或 2 秒超时：

```c
uint32_t query_sequence;
BaseType_t is_query_reply;

while (1) {
    /* 收到任务通知立即返回；没有命令时等待 2 秒后返回。 */
    is_query_reply = xTaskNotifyWait(0U,
                                    UINT32_MAX,
                                    &query_sequence,
                                    pdMS_TO_TICKS(2000U));

    xSemaphoreTake(snapshotMutex, portMAX_DELAY);
    local = g_snapshot;
    xSemaphoreGive(snapshotMutex);

    data[0] = 1U;
    data[1] = sequence++;
    CanProtocol_PutU16BE(&data[2], local.adc);
    CanProtocol_PutU16BE(&data[4], (uint16_t)local.light_lux);

    data[6] = 0U;
    if (local.adc_valid != 0U) {
        data[6] |= 0x01U;
    }
    if (local.light_valid != 0U) {
        data[6] |= 0x02U;
    }

    if (is_query_reply == pdTRUE) {
        data[6] |= 0x80U;
        data[7] = (uint8_t)query_sequence;
    } else {
        data[7] = 0U;
    }

    (void)CanBasic_Send(CAN_ID_S1_TELEMETRY, data, 8U);
}
```

## 第四步：Slave 2 使用同一套结构

在 `slave_node2/Core/Src/app_sensor.c` 增加并创建 `Slave2CommandTask`，代码如下：

```c
void Slave2CommandTask(void *pvParameters)
{
    CanRxFrame_t frame;

    (void)pvParameters;

    while (1) {
        if (!CanRxQueue_Get(&frame, portMAX_DELAY)) {
            continue;
        }

        if ((frame.id == CAN_ID_S2_COMMAND) &&
            (frame.dlc == 8U) &&
            (frame.data[0] == 1U) &&
            (frame.data[2] == CAN_COMMAND_QUERY_NOW)) {
            xTaskNotify(Slave2CanTask_Handle,
                        (uint32_t)frame.data[1],
                        eSetValueWithOverwrite);
        }
    }
}
```

`Slave2CanTask()` 也使用同样的 `xTaskNotifyWait(..., 2000 ms)`。区别只有：

- 继续使用 `CanProtocol_PutS16BE(&data[4], temperature_x10)` 组装温度。
- 查询回复时同样设置 `data[6] |= 0x80U`，并把命令序号放入 `data[7]`。
- 最后发送 ID 仍是 `CAN_ID_S2_TELEMETRY`。

不要再创建新的 `TelemetryTask`；Day 8、Day 9 已有的两个 CAN 任务同时承担“周期超时发送”和“收到通知立即发送”。

## 第五步：Master 显示查询回复标记

Master 的 `CanTask()` 解码帧时增加：

```c
if (frame.id == CAN_ID_S1_TELEMETRY) {
    OLED_ShowChar(1, 16, ((frame.data[6] & 0x80U) != 0U) ? 'Q' : ' ');
} else if (frame.id == CAN_ID_S2_TELEMETRY) {
    OLED_ShowChar(3, 16, ((frame.data[6] & 0x80U) != 0U) ? 'Q' : ' ');
}
```

## 今天怎样验证

- [ ] 不按键时，Master 每约 2 秒收到一帧 `0x180` 和一帧 `0x181`。
- [ ] 按 PB1 后，只有 Slave 1 在下一个 2 秒周期前立即回复。
- [ ] 按 PB11 后，只有 Slave 2 在下一个 2 秒周期前立即回复。
- [ ] 查询回复的 Byte 6 bit7 为 1，Byte 7 等于对应节点的命令序号。
- [ ] 下一次周期帧的 Byte 6 bit7 恢复为 0。
- [ ] 拔掉一个从节点时，另一个从节点的周期上报和查询回复不受影响。

---

# Day 12：离线检测 + Slave 2 高温告警帧

## 今天做完后应当看到什么

1. Slave 1 或 Slave 2 断电（或拔掉 CAN 收发器）后，Master 最多约 5 秒显示 `S1 OFFLINE` 或 `S2 OFFLINE`。
2. Slave 2 温度首次达到 `30.0°C` 时，立即发送一帧 `0x081` 高温告警；Master 显示 `TEMP HIGH`。
3. 高温状态持续期间，Slave 2 仍照常每 2 秒发送 `0x181` 遥测帧，但不重复刷屏、也不连续发送 `0x081`。
4. 温度降到 `28.0°C` 或更低时，Slave 2 发送一帧“告警解除”事件；Master 清除 `TEMP HIGH`。

本日把两种东西分开：

| 帧 | 含义 | 发送时机 |
| --- | --- | --- |
| `0x181` | Slave 2 常规遥测，`Byte 6 bit2` 表示“此刻仍高温” | 周期上报或查询回复 |
| `0x081` | Slave 2 高温事件，通知“刚进入高温”或“刚解除高温” | 状态发生变化时，仅一次 |

`0x181` 是状态；`0x081` 是事件。不能只设置 `bit2` 却称为“告警帧”。

## 先固定协议，不要边写边改

Day 5 的 `CAN_ID_S2_ALARM` 已经是 `0x081`，今天定义它的完整 8 字节格式：

| 字节 | 含义 |
| --- | --- |
| Byte 0 | 协议版本，固定 `1` |
| Byte 1 | 告警事件序号，每发生一次进入/解除事件加 1 |
| Byte 2~3 | 触发事件时的温度，`int16_t` 大端，单位 `0.1°C`；例如 `300` 是 30.0°C |
| Byte 4 | 事件类型：`0x01` = 高温进入；`0x00` = 高温解除 |
| Byte 5 | 告警源，固定 `0x02`，表示 Slave 2 |
| Byte 6~7 | 保留，固定 `0` |

阈值固定为：`>= 30.0°C` 进入高温，`<= 28.0°C` 解除高温。28.0~30.0°C 之间保持原状态，这 2°C 的迟滞用于避免温度在 30°C 附近抖动时反复报警。

## 第一步：Slave 2 增加告警状态与发送函数

打开 `slave_node2/Core/Src/app_sensor.c`。在任务声明下方、任意函数定义之前增加：

```c
#define S2_TEMP_ALARM_ON_X10   300     /* 30.0°C */
#define S2_TEMP_ALARM_OFF_X10  280     /* 28.0°C */

static uint8_t s_temp_alarm_active = 0U;
static uint8_t s_alarm_event_sequence = 0U;

static void Slave2Can_SendTempAlarm(int16_t temperature_x10, uint8_t alarm_active)
{
    uint8_t alarm[8] = {0};

    alarm[0] = 1U;
    alarm[1] = s_alarm_event_sequence++;
    CanProtocol_PutS16BE(&alarm[2], temperature_x10);
    alarm[4] = (alarm_active != 0U) ? 0x01U : 0x00U;
    alarm[5] = 0x02U;

    /* 发送失败不改变 s_temp_alarm_active；Day 14 故障测试时统一观察。 */
    (void)CanBasic_Send(CAN_ID_S2_ALARM, alarm, 8U);
}

/* 温度有效时调用；只在“进入”或“解除”时发送一次事件帧。 */
static uint8_t Slave2Can_UpdateTempAlarm(int16_t temperature_x10)
{
    if ((s_temp_alarm_active == 0U) &&
        (temperature_x10 >= S2_TEMP_ALARM_ON_X10)) {
        s_temp_alarm_active = 1U;
        Slave2Can_SendTempAlarm(temperature_x10, 1U);
    } else if ((s_temp_alarm_active != 0U) &&
               (temperature_x10 <= S2_TEMP_ALARM_OFF_X10)) {
        s_temp_alarm_active = 0U;
        Slave2Can_SendTempAlarm(temperature_x10, 0U);
    }

    return s_temp_alarm_active;
}
```

这里的 `temperature_x10` 是函数形参，不是可直接在任务中使用的变量；下一步必须先在任务里创建它。这正是之前编译报“identifier undefined”的原因。

## 第二步：替换 Slave 2 的 `Slave2CanTask()`

用下面完整函数替换 `slave_node2/Core/Src/app_sensor.c` 原来的 `Slave2CanTask()`。保留后面的 `Slave2CommandTask()` 不动。

```c
void Slave2CanTask(void *pvParameters)
{
    uint8_t data[8] = {0};
    uint8_t sequence = 0U;
    uint32_t query_sequence = 0U;
    BaseType_t is_query_reply;
    SensorSnapshot_t local;
    int16_t temperature_x10;

    (void)pvParameters;

    while (1) {
        is_query_reply = xTaskNotifyWait(0U,
                                         UINT32_MAX,
                                         &query_sequence,
                                         pdMS_TO_TICKS(2000U));

        xSemaphoreTake(snapshotMutex, portMAX_DELAY);
        local = g_snapshot;
        xSemaphoreGive(snapshotMutex);

        temperature_x10 = (int16_t)(local.temp * 10.0f);

        data[0] = 1U;
        data[1] = sequence++;
        CanProtocol_PutU16BE(&data[2], local.adc);
        CanProtocol_PutS16BE(&data[4], temperature_x10);

        data[6] = 0U;
        if (local.adc_valid != 0U) {
            data[6] |= 0x01U;       /* bit0：ADC 有效 */
        }
        if (local.temp_valid != 0U) {
            data[6] |= 0x04U;       /* bit2：温度有效 */

            if (Slave2Can_UpdateTempAlarm(temperature_x10) != 0U) {
                data[6] |= 0x08U;   /* bit3：当前正处于高温 */
            }
        } else {
            /* 温度传感器失效不能保留旧告警；不发送“解除”事件，因为温度未知。 */
            s_temp_alarm_active = 0U;
        }

        if (is_query_reply == pdTRUE) {
            data[6] |= 0x80U;       /* bit7：查询回复 */
            data[7] = (uint8_t)query_sequence;
        } else {
            data[7] = 0U;
        }

        (void)CanBasic_Send(CAN_ID_S2_TELEMETRY, data, 8U);
    }
}
```

注意：从今天起 `Byte 6 bit2` 是“温度数据有效”，和 Day 9 保持一致；`bit3` 才是“当前高温”。不要把 `bit2` 改成高温标志。

## 第三步：确认 Master 的过滤器已放行 `0x081`

本项目当前 Master 使用 **16 位列表过滤器**，一个 bank 可精确列出四个标准帧。打开 `master_node/Core/Src/can_basic.c`，确认 `CanBasic_ConfigMasterFilter()` 是下面这四项；如果你前面已经按 Day 5 配好，就**不用改**：

```c
filter.FilterMode = CAN_FILTERMODE_IDLIST;
filter.FilterScale = CAN_FILTERSCALE_16BIT;
filter.FilterIdHigh = (uint16_t)(CAN_ID_S1_TELEMETRY << 5); /* 0x180 */
filter.FilterIdLow = (uint16_t)(CAN_ID_S2_TELEMETRY << 5);  /* 0x181 */
filter.FilterMaskIdHigh = (uint16_t)(CAN_ID_S2_ALARM << 5); /* 0x081 */
filter.FilterMaskIdLow = 0xFFFFU;                           /* 第 4 项占位 */
```

这里 `FilterMaskIdHigh/Low` 在**列表模式**里名称虽带 `Mask`，实际是第 3、4 个 ID 位置，并不是掩码。不要把这一版换成宽掩码过滤器。

## 第四步：Master 保存告警状态并显示

在 Master 的 `MasterCanData_t` 中增加：

```c
uint8_t s2_temp_alarm;
TickType_t s1_last_tick;
TickType_t s2_last_tick;
uint8_t s1_online;
uint8_t s2_online;
```

然后用下面完整函数替换 Master 的 `CanTask()`：

```c
void CanTask(void *pvParameters)
{
    CanRxFrame_t frame;
    MasterCanData_t latest = {0};

    (void)pvParameters;

    while (1) {
        if (CanRxQueue_Get(&frame, pdMS_TO_TICKS(200U))) {
            if ((frame.id == CAN_ID_S1_TELEMETRY) &&
                (frame.dlc == 8U) && (frame.data[0] == 1U)) {
                latest.s1_adc = CanProtocol_GetU16BE(&frame.data[2]);
                latest.s1_light = CanProtocol_GetU16BE(&frame.data[4]);
                latest.s1_status = frame.data[6];
                latest.s1_last_tick = xTaskGetTickCount();
                latest.s1_online = 1U;
            } else if ((frame.id == CAN_ID_S2_TELEMETRY) &&
                       (frame.dlc == 8U) && (frame.data[0] == 1U)) {
                latest.s2_adc = CanProtocol_GetU16BE(&frame.data[2]);
                latest.s2_temp_x10 = CanProtocol_GetS16BE(&frame.data[4]);
                latest.s2_status = frame.data[6];
                latest.s2_temp_alarm =
                    ((frame.data[6] & 0x08U) != 0U) ? 1U : 0U;
                latest.s2_last_tick = xTaskGetTickCount();
                latest.s2_online = 1U;
            } else if ((frame.id == CAN_ID_S2_ALARM) &&
                       (frame.dlc == 8U) && (frame.data[0] == 1U) &&
                       (frame.data[5] == 0x02U)) {
                latest.s2_temp_x10 = CanProtocol_GetS16BE(&frame.data[2]);
                latest.s2_temp_alarm = (frame.data[4] == 0x01U) ? 1U : 0U;
            }
        }

        if ((latest.s1_online != 0U) &&
            ((xTaskGetTickCount() - latest.s1_last_tick) > pdMS_TO_TICKS(5000U))) {
            latest.s1_online = 0U;
        }
        if ((latest.s2_online != 0U) &&
            ((xTaskGetTickCount() - latest.s2_last_tick) > pdMS_TO_TICKS(5000U))) {
            latest.s2_online = 0U;
            latest.s2_temp_alarm = 0U;
        }

        if (latest.s1_online != 0U) {
            OLED_ShowString(1, 1, "S1AD:");
            OLED_ShowNum(1, 7, latest.s1_adc, 4);
            OLED_ShowString(2, 1, "LIGHT:");
            OLED_ShowNum(2, 7, latest.s1_light, 5);
            OLED_ShowChar(1, 16,
                ((latest.s1_status & 0x80U) != 0U) ? 'Q' : ' ');
        } else {
            OLED_ShowString(1, 1, "S1 OFFLINE     ");
            OLED_ShowString(2, 1, "                ");
        }

        if (latest.s2_online != 0U) {
            OLED_ShowString(3, 1, "S2AD:");
            OLED_ShowNum(3, 7, latest.s2_adc, 4);
            OLED_ShowString(4, 1,
                (latest.s2_temp_alarm != 0U) ? "TEMP HIGH       " : "T*10:           ");
            OLED_ShowSignedNum(4, 7, latest.s2_temp_x10, 4);
            OLED_ShowChar(3, 16,
                ((latest.s2_status & 0x80U) != 0U) ? 'Q' : ' ');
        } else {
            OLED_ShowString(3, 1, "S2 OFFLINE     ");
            OLED_ShowString(4, 1, "                ");
        }
    }
}
```

只有 `CanTask()` 操作 Master OLED；不要另开一个“告警 OLED 任务”，否则两个任务会互相覆盖屏幕。

## 第五步：验收顺序

1. 三块板先保持常温，Master 应持续显示 S1、S2 数据，且没有 `TEMP HIGH`。
2. 让 Slave 2 温度达到或超过 30.0°C：Master 第 4 行应变为 `TEMP HIGH`，温度仍显示在第 7 列。
3. 保持高温 6 秒：界面继续显示高温，但不会每 2 秒反复出现新的告警事件；`0x181` 仍正常周期发送。
4. 温度降到 29.0°C：仍必须显示 `TEMP HIGH`，因为尚未达到 28.0°C 解除阈值。
5. 温度降到 28.0°C 或更低：第 4 行恢复 `T*10:`，并显示当前温度。
6. 给 Slave 2 断电：最多约 5 秒显示 `S2 OFFLINE`，同时清除旧的高温提示。
7. 再给 Slave 2 上电：收到下一帧 `0x181` 后恢复在线显示。

## 本日常见错误

- `temperature_x10 undeclared`：在 `Slave2CanTask()` 中缺少 `int16_t temperature_x10;` 或没有在组帧前赋值。
- 高温永远不显示：Master 过滤器没有接收 `0x081`，或你把高温状态错写到 `Byte 6 bit2`。
- 温度到 29°C 就解除：迟滞解除阈值应为 `280`，不是 `300`。
- OLED 的 `Q` 被覆盖：必须像本节完整代码一样，在刷新第 1/3 行字符串和数值之后再绘制 `Q`。
- 断电后仍显示 `TEMP HIGH`：离线分支必须执行 `latest.s2_temp_alarm = 0U;`。

---

# Day 13：按键查询的响应时间和超时

## 今天只解决一个问题

Day 11 已经能让 PB1 查询 Slave 1、PB11 查询 Slave 2。今天只给这条请求/回复链路加一个最小判断：

```text
按键发送命令
  -> 记录目标节点、命令序号、发送时刻
  -> 收到相同序号的查询回复：打印响应时间
  -> 500 ms 没收到：打印 timeout
```

不做最大值、平均值、重复帧、遥测跳号等统计。那些内容放到 Day 14 作为可选压力测试。

`CanBasic_Send()` 返回 `true` 只表示命令进入了 CAN 发送邮箱，不代表从节点已经收到；只有收到
bit7=1 且 Byte7 序号正确的回复，才算本次查询成功。

## 第一步：增加一个等待状态

当前一次只允许等待一个节点回复，这样代码最容易看懂。在 Master `app_sensor.c` 的结构体定义后增加：

```c
typedef struct {
    uint8_t pending;       /* 1=正在等待回复，0=当前没有查询。 */
    uint8_t node;          /* 1=Slave 1，2=Slave 2。 */
    uint8_t sequence;      /* 本次命令 Byte1。 */
    TickType_t sent_tick;  /* 命令发送时刻。 */
} QueryWait_t;

static QueryWait_t s_query_wait;
```

`ButtonTask` 和 `CanTask` 都会访问它，所以读写这四个字段时使用短临界区。临界区内只赋值，
不能在里面发送 CAN、打印串口或延时。

## 第二步：修改 `ButtonTask()`

保留你当前的两个独立命令序号。在 `ButtonTask()` 的循环中，把发送部分改成：

```c
uint16_t command_id;
uint8_t target_node;
uint8_t can_send_ok;

key = KEY_INPUT();

/* 上一次查询结束前不接受第二次查询，避免序号被覆盖。 */
if ((key != 0U) && (s_query_wait.pending == 0U)) {
    if (key == 1U) {
        target_node = 1U;
        command_id = CAN_ID_S1_COMMAND;
        command[1] = command_sequence_s1++;
    } else {
        target_node = 2U;
        command_id = CAN_ID_S2_COMMAND;
        command[1] = command_sequence_s2++;
    }

    /* 先登记等待状态，防止很快到达的回复抢在登记前被 CanTask 处理。 */
    taskENTER_CRITICAL();
    s_query_wait.pending = 1U;
    s_query_wait.node = target_node;
    s_query_wait.sequence = command[1];
    s_query_wait.sent_tick = xTaskGetTickCount();
    taskEXIT_CRITICAL();

    can_send_ok = CanBasic_Send(command_id, command, 8U) ? 1U : 0U;
    if (can_send_ok == 0U) {
        taskENTER_CRITICAL();
        s_query_wait.pending = 0U;
        taskEXIT_CRITICAL();
        Serial_Printf("S%u query mailbox failed\r\n", target_node);
    }
}

vTaskDelay(pdMS_TO_TICKS(10U));
```

为了让主循环更清楚，把 `command_id`、`target_node`、`can_send_ok` 放在 `ButtonTask()` 的局部变量区，
不要在 `if` 中间重复定义。

## 第三步：`CanTask()` 收到正确回复时打印耗时

在 `CanTask()` 的局部变量区增加：

```c
uint8_t reply_node;
uint8_t reply_matched;
uint32_t latency_ms;
```

先保存本轮是否真的收到新帧。队列等待超时时，不能继续使用上一次的 `frame` 判断查询回复：

```c
uint8_t frame_received;

frame_received = CanRxQueue_Get(&frame, pdMS_TO_TICKS(200U)) ? 1U : 0U;
if (frame_received != 0U) {
    /* 在这里解析 0x180、0x181 和 0x081。 */
}
```

在收到新帧并完成普通遥测解析后，增加下面这一段。除了 bit7，还要检查 DLC 和协议版本：

```c
if ((frame_received != 0U) &&
    (frame.dlc == 8U) &&
    (frame.data[0] == 1U) &&
    ((frame.id == CAN_ID_S1_TELEMETRY) ||
     (frame.id == CAN_ID_S2_TELEMETRY)) &&
    ((frame.data[6] & 0x80U) != 0U)) {

    reply_node = (frame.id == CAN_ID_S1_TELEMETRY) ? 1U : 2U;
    reply_matched = 0U;
    latency_ms = 0U;

    taskENTER_CRITICAL();
    if ((s_query_wait.pending != 0U) &&
        (s_query_wait.node == reply_node) &&
        (s_query_wait.sequence == frame.data[7])) {
        latency_ms = (uint32_t)(xTaskGetTickCount() -
                                s_query_wait.sent_tick);
        s_query_wait.pending = 0U;
        reply_matched = 1U;
    }
    taskEXIT_CRITICAL();

    if (reply_matched != 0U) {
        Serial_Printf("S%u reply seq=%u latency=%lu ms\r\n",
                      reply_node,
                      frame.data[7],
                      (unsigned long)latency_ms);
    }
}
```

例如串口打印：

```text
S1 reply seq=4 latency=12 ms
S2 reply seq=2 latency=18 ms
```

## 第四步：500 ms 没回复就打印超时

在 `CanTask()` 局部变量区再增加：

```c
uint8_t timeout_node;
uint8_t timeout_sequence;
uint8_t timed_out;
```

在 `CanTask()` 的 `while (1)` 末尾增加：

```c
timed_out = 0U;
timeout_node = 0U;
timeout_sequence = 0U;

taskENTER_CRITICAL();
if ((s_query_wait.pending != 0U) &&
    ((xTaskGetTickCount() - s_query_wait.sent_tick) >
     pdMS_TO_TICKS(500U))) {
    timeout_node = s_query_wait.node;
    timeout_sequence = s_query_wait.sequence;
    s_query_wait.pending = 0U;
    timed_out = 1U;
}
taskEXIT_CRITICAL();

if (timed_out != 0U) {
    Serial_Printf("S%u timeout seq=%u\r\n",
                  timeout_node,
                  timeout_sequence);
}
```

`CanTask()` 每次最多等待接收队列 200 ms，因此实际打印可能出现在约 500~700 ms，这是正常的。
超时以后 `pending` 被清零，可以继续按另一个按键查询。

## 今天怎样验证

- [ ] 按 PB1，串口打印 `S1 reply`、正确序号和响应时间。
- [ ] 按 PB11，串口打印 `S2 reply`、正确序号和响应时间。
- [ ] 正常连接时，连续各查询 10 次都没有 timeout。
- [ ] 拔掉 Slave 1 后按 PB1，约 500~700 ms 打印 `S1 timeout`。
- [ ] S1 超时后仍能按 PB11 查询 Slave 2。
- [ ] 周期遥测和 OLED 显示不受查询计时影响。

## 留到 Day 14 的可选诊断

下面内容今天不写代码，只在长时间测试确实需要时再加：

- 遥测 Byte1 跳号、重复帧统计。
- 最大/平均响应时间。
- `CanRxQueue_GetDropCount()` 压力测试。
- `HAL_CAN_ErrorCallback()` 错误分类。

---

# Day 14：测试

按顺序测试，前一个不通过不做后一个：

- [ ] Loopback 1000 帧。
- [ ] 两板请求/回复 10 分钟。
- [ ] 三板广播/两从机回复 15 分钟。
- [ ] 过滤器：`0x100` 只进 Slave 1，`0x101` 只进 Slave 2，`0x555` 不进应用。
- [ ] Slave 1 ADC、光照在 Master 改变。
- [ ] Slave 2 ADC、温度在 Master 改变。
- [ ] PB1 只触发 Slave 1 回复，PB11 只触发 Slave 2 回复。
- [ ] 两个节点各查询 20 次，正常连接时都能收到对应节点的回复，不出现 timeout。
- [ ] 串口打印的节点号和序号与本次按键查询一致。
- [ ] 拔掉 Slave 1 后按 PB1，产生 S1 timeout；Slave 2 的周期帧和查询不受影响。
- [ ] 拔掉 Slave 2 后按 PB11，产生 S2 timeout；Slave 1 的周期帧和查询不受影响。
- [ ] 关闭任一从机，5 秒左右显示离线。
- [ ] 运行 2 小时，OLED 持续刷新，两个从节点的周期数据都没有停止。

可选压力测试：需要进一步排查稳定性时，再统计接收队列丢帧、遥测序号连续性和最大/平均响应时间。它们不是 Day 13 的必做代码。

---

# Day 15：整理

最终删除或用 `#if 0` 关闭 Day 2 到 Day 4 的 Loopback/Echo 临时代码；保留 Day 5 以后实际使用的 `can_basic.c`、`can_protocol.c`、`can_rx_queue.c` 和节点文件。

你面试时只需要按教程顺序讲：

1. 先用 Loopback 验证 CAN 外设。
2. 用两板、三板 Normal 模式验证物理总线。
3. 用硬件过滤器让节点只收自己的帧。
4. 用 FIFO0 中断接收。
5. 因为 FreeRTOS 下可能连续来帧，所以把教程的单帧变量换成队列。
6. 最后让从节点平时按 2 秒周期上报；PB1/PB11 分别用 `0x100/0x101` 请求立即回复，并通过回显序号计算响应时间、判断超时。
