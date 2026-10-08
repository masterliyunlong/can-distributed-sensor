# CAN 分布式数据采集系统：7 周可执行开发计划

> 起点：`CAN项目实战详细计划.md` 中的项目一已经完成。  
> 本计划从项目二开始，不再安排 GPIO、UART、ADC、BH1750、OLED、DS18B20 的首次驱动开发。  
> 周期：7 周，35 个工作日；每天 3～5 小时，周末用于补测或休息。  
> 终点：完成一主两从 CAN + FreeRTOS 系统、测试报告、演示材料、项目总结和面试准备。

---

## 导航

- 第 1 周（Day 1～5）：FreeRTOS 单板多任务
- 第 2 周（Day 6～10）：CAN 数据链路
- 第 3 周（Day 11～15）：FreeRTOS 与 CAN 集成
- 第 4 周（Day 16～20）：完整业务闭环
- 第 5 周（Day 21～25）：可靠性与资源收尾
- 第 6 周（Day 26～30）：自动化测试与项目交付
- 第 7 周（Day 31～35）：总结、简历和面试

默认 Day 1 是启动本计划后的第一个周一，Day 1～5 对应周一至周五；以后各周相同。若从周中开始，保持 Day 的先后顺序即可，不压缩两个工作日到同一天。

---

## 1. 最终要交付什么

### 1.1 系统功能

使用三块 STM32F103C8T6 组成一条 500 kbit/s Classic CAN 总线：

| 节点 | 外设 | 最终职责 |
|---|---|---|
| Master | OLED、DS18B20、按键、UART、CAN | 接收两个从节点的遥测，显示数据，判断在线状态，发送查询命令，记录系统统计 |
| Slave 1 | BH1750、电位器 ADC、UART、CAN | 每 500 ms 上报光照与 ADC，响应查询命令，上报传感器状态 |
| Slave 2 | DS18B20、电位器 ADC、UART、CAN | 每 500 ms 上报温度与 ADC，响应查询命令，产生高温告警 |

必须完成以下可演示行为：

1. 两个从节点每 500 ms 向 Master 上报一次遥测；
2. Master OLED 同时显示 Slave 1、Slave 2 和本地状态；
3. 按下 Master 按键后，指定从节点立即回复一次遥测；
4. 任一从节点断电 3 秒后，Master 显示 `OFFLINE`；
5. 从节点重新上电并发送有效帧后，Master 自动显示 `ONLINE`；
6. Slave 2 温度越过阈值时发送告警，回落到解除阈值后清除告警；
7. 串口能输出结构化运行日志和错误计数；
8. CAN 接收中断只取帧并入队，协议解析在任务中完成；
9. 系统连续运行 24 小时，有明确的收发计数、丢弃计数和资源水位；
10. 仓库中包含 README、协议说明、测试报告、Bug 复盘和演示视频。

### 1.2 最终仓库结构

第 1 周开始时先建立以下目录；每天的产出放进对应位置：

```text
can-distributed-system/
├─ firmware/
│  ├─ common/
│  │  ├─ can_protocol.c/.h
│  │  ├─ app_types.h
│  │  └─ crc_or_utils.c/.h
│  ├─ master/
│  │  ├─ Core/
│  │  └─ MDK-ARM/
│  ├─ slave1/
│  │  ├─ Core/
│  │  └─ MDK-ARM/
│  └─ slave2/
│     ├─ Core/
│     └─ MDK-ARM/
├─ exercises/
│  ├─ freertos-single-board/
│  ├─ can-loopback/
│  └─ can-two-node/
├─ tools/
│  ├─ serial_logger.py
│  ├─ analyze_log.py
│  └─ requirements.txt
├─ tests/
│  ├─ test_cases.md
│  ├─ test_records.csv
│  └─ protocol_vectors.md
├─ docs/
│  ├─ architecture.md
│  ├─ can_protocol.md
│  ├─ hardware_wiring.md
│  ├─ resource_report.md
│  ├─ bugs/
│  └─ images/
├─ artifacts/
│  ├─ firmware/
│  ├─ logs/
│  └─ release-checklist.md
├─ README.md
└─ CHANGELOG.md
```

`exercises` 保存项目二、项目三的阶段练习；`firmware` 只保存最终三节点工程。这样可以保留学习过程，也不会把最终代码和练习代码混在一起。

### 1.3 7 周路线

| 周次 | 对应项目 | 本周结果 | 周验收 |
|---|---|---|---|
| 第 1 周 | 项目二：FreeRTOS 多任务传感器 | 单板完成任务、队列、互斥锁、信号量和资源统计 | 单板连续运行 2 小时，无 HardFault，四类数据稳定更新 |
| 第 2 周 | 项目三：CAN 数据链路 | 完成单板回环、双板互通、三板总线和过滤器 | 三节点各发 10,000 帧，ID 与计数正确 |
| 第 3 周 | 项目四：FreeRTOS + CAN | 完成协议模块、CAN RX ISR、RX/TX 队列和三节点基础遥测 | Master 连续接收两个从节点 1 小时 |
| 第 4 周 | 项目五：完整业务 | 完成显示、查询、在线检测、恢复、告警和日志 | 所有用户功能逐条通过 |
| 第 5 周 | 项目五：可靠性收尾 | 完成统计、压力、故障注入、看门狗和资源测量 | 异常场景均有可观察结果和恢复路径 |
| 第 6 周 | 测试与交付 | 完成 24 小时测试、脚本、README、报告和演示视频 | 发布 `v1.0.0`，他人可按 README 复现 |
| 第 7 周 | 总结与面试 | 完成项目总结、简历描述、Bug 故事和两轮模拟面试 | 1 分钟、5 分钟、15 分钟三种讲法都能完成 |

---

## 2. 固定技术方案

后续每天都按本节接口推进，避免做到中途反复改协议和目录。

### 2.1 CAN 位时序和物理连接

STM32F103 的 CAN 时钟来自 APB1，按 36 MHz 配置：

```text
Prescaler = 4
Time Segment 1 = 15 TQ
Time Segment 2 = 2 TQ
SJW = 1 TQ
总 TQ = 1 + 15 + 2 = 18
Bitrate = 36 MHz / 4 / 18 = 500 kbit/s
采样点 = (1 + 15) / 18 = 88.9%
```

三块板使用 PB8/CAN_RX、PB9/CAN_TX，并在 CubeMX 中启用 CAN remap。物理总线按线型连接：

```text
120 Ω                                             120 Ω
  |                                                 |
  +------ Master ------ Slave 1 ------ Slave 2 -----+
          CAN_H ---------- CAN_H ---------- CAN_H
          CAN_L ---------- CAN_L ---------- CAN_L
          GND   ---------- GND   ---------- GND
```

第 2 周第 6 天会实际确认收发器供电与逻辑电平。若使用裸 TJA1050，VCC 按器件要求使用 5 V，并确认 RXD 到 STM32 的电平安全；若模块没有电平处理，换用 3.3 V 逻辑兼容的 CAN 收发器后再进入 Normal 模式测试。

### 2.2 CAN ID 与数据定义

| CAN ID | 方向 | 内容 | 触发方式 | DLC |
|---:|---|---|---|---:|
| `0x080` | Slave 1 -> Master | Slave 1 告警/故障事件 | 状态变化 | 8 |
| `0x081` | Slave 2 -> Master | Slave 2 告警/故障事件 | 状态变化 | 8 |
| `0x100` | Master -> Slave 1 | Slave 1 查询命令 | 按键/测试 | 8 |
| `0x101` | Master -> Slave 2 | Slave 2 查询命令 | 按键/测试 | 8 |
| `0x180` | Slave 1 -> Master | ADC + 光照遥测 | 500 ms/查询 | 8 |
| `0x181` | Slave 2 -> Master | ADC + 温度遥测 | 500 ms/查询 | 8 |

Slave 1 遥测 `0x180`：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | `version` | 固定为 1 |
| 1 | `telemetry_seq` | 每发一帧加 1，自然回绕 |
| 2～3 | `adc` | `uint16_t`，大端，范围 0～4095 |
| 4～5 | `light_lux` | `uint16_t`，大端；无效为 `0xFFFF` |
| 6 | `status` | bit0 ADC 有效，bit1 光照有效，bit2 告警，bit3 CAN 错误，bit4 队列丢弃 |
| 7 | `last_cmd_seq` | 最近处理的命令序号，尚未收到命令时为 `0xFF` |

Slave 2 遥测 `0x181`：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | `version` | 固定为 1 |
| 1 | `telemetry_seq` | 每发一帧加 1 |
| 2～3 | `adc` | `uint16_t`，大端 |
| 4～5 | `temperature_x10` | `int16_t`，大端，253 表示 25.3 ℃ |
| 6 | `status` | bit0 ADC 有效，bit1温度有效，bit2 告警，bit3 CAN 错误，bit4 队列丢弃 |
| 7 | `last_cmd_seq` | 最近处理的命令序号 |

命令帧 `0x100/0x101`：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | `version` | 固定为 1 |
| 1 | `cmd_seq` | Master 每发命令加 1 |
| 2 | `cmd` | `0x01` 表示立即上报 |
| 3～7 | `parameter/reserved` | 当前全部置 0 |

告警帧 `0x080/0x081`：

| Byte | 字段 | 说明 |
|---:|---|---|
| 0 | `version` | 固定为 1 |
| 1 | `event_seq` | 每个事件加 1 |
| 2 | `alarm_code` | `1` 传感器故障，`2` 高温 |
| 3 | `state` | `1` 激活，`0` 解除 |
| 4～5 | `value` | 当前测量值，大端 |
| 6 | `status` | 与遥测状态位一致 |
| 7 | `reserved` | 置 0 |

协议代码只通过逐字节编码和解码，不直接把 C 结构体强制转换成 8 字节发送。

### 2.3 FreeRTOS 对象和任务

应用层统一使用原生 FreeRTOS API。初始配置如下，最终栈大小在第 5 周按实测水位调整：

#### Master

| 任务 | 周期/触发 | 优先级 | 初始栈 | 职责 |
|---|---|---:|---:|---|
| `CanRxTask` | RX 队列触发 | 4 | 192 words | 校验、解码、更新节点状态 |
| `CanTxTask` | TX 队列触发 | 3 | 128 words | 唯一调用 `HAL_CAN_AddTxMessage` 的任务 |
| `LocalTempTask` | 1000 ms | 2 | 128 words | 启动并读取本地 DS18B20 |
| `UiTask` | 200 ms | 1 | 192 words | 复制快照并刷新 OLED |
| `OnlineTask` | 200 ms | 2 | 96 words | 检查两个从节点超时与恢复 |
| `ButtonTask` | 信号量触发 | 2 | 96 words | 消抖并生成查询命令 |
| `MonitorTask` | 5000 ms | 1 | 160 words | 输出计数、栈水位、堆余量 |
| `LogTask` | 日志队列触发 | 1 | 192 words | 唯一操作 UART 的任务 |

#### Slave 1

| 任务 | 周期/触发 | 优先级 | 初始栈 | 职责 |
|---|---|---:|---:|---|
| `CanRxTask` | RX 队列触发 | 4 | 128 words | 解码 Master 命令 |
| `CanTxTask` | TX 队列触发 | 3 | 128 words | 发送遥测和事件 |
| `AdcTask` | 100 ms | 2 | 96 words | 读取 DMA 最新值并滤波 |
| `LightTask` | 500 ms | 2 | 128 words | 分阶段读取 BH1750 |
| `TelemetryTask` | 500 ms/通知 | 2 | 128 words | 取快照、编码并放入 TX 队列 |
| `MonitorTask` | 5000 ms | 1 | 160 words | 输出资源与错误统计 |
| `LogTask` | 日志队列触发 | 1 | 192 words | 串口输出 |

#### Slave 2

结构与 Slave 1 相同，将 `LightTask` 换成 `TempTask`；`TempTask` 每 1000 ms 完成一次 DS18B20 转换，`AlarmTask` 可合并进 `TempTask`，在温度更新后执行状态机。

#### 队列和同步对象

| 对象 | 长度 | 元素 | 用途 |
|---|---:|---|---|
| `canRxQueue` | 16 | `CanFrame_t` | ISR 向 `CanRxTask` 传帧 |
| `canTxQueue` | 16 | `CanFrame_t` | 业务任务向 `CanTxTask` 交帧 |
| `logQueue` | 16 | 固定长 `LogMsg_t` | 各任务向 `LogTask` 交日志 |
| `buttonSem` | 二值 | 无 | EXTI ISR 唤醒 `ButtonTask` |
| `snapshotMutex` | 1 | 无 | 保护系统快照的短时间复制 |
| `i2cMutex` | 1 | 无 | 第 1 周单板练习时协调 BH1750 与 OLED |

最终初始堆配置为 Master 10 KB、Slave 1/2 各 8 KB；`LogMsg_t` 控制为 64 字节。这个数值是第一次能上板的预算，不是最终结论。Day 25 根据 map 文件、`xPortGetMinimumEverFreeHeapSize()` 和各任务 `uxTaskGetStackHighWaterMark()` 调整，并至少保留 1 KB 最小剩余堆和 40 words 最小栈水位。

核心数据类型：

```c
typedef struct {
    uint32_t id;
    uint8_t dlc;
    uint8_t data[8];
    uint32_t rx_tick;
} CanFrame_t;

typedef struct {
    uint16_t adc;
    uint16_t light_lux;
    int16_t temperature_x10;
    uint8_t status;
    uint32_t update_tick;
} SensorSnapshot_t;

typedef struct {
    SensorSnapshot_t slave1;
    SensorSnapshot_t slave2;
    int16_t local_temperature_x10;
    bool slave1_online;
    bool slave2_online;
} MasterSnapshot_t;
```

---

## 3. 每天固定执行格式

每个工作日按四个时间段推进：

| 时间段 | 时长 | 执行动作 |
|---|---:|---|
| A：开工 | 15 分钟 | 打开昨日记录；写下今天唯一验收目标；确认当前可回退 commit |
| B：实现 | 90～120 分钟 | 只实现当天主链路；编译通过后立即提交一次 |
| C：上板测试 | 60～90 分钟 | 按当天用例烧录、接线、记录日志与计数 |
| D：收尾 | 30～45 分钟 | 保存证据、更新文档、提交 Git、写明次日第一步 |

当天没有达到验收条件时，不把未通过项标成完成。次日先安排 90 分钟继续定位；仍未通过则使用该周周末缓冲，不同时开启后续两个新模块。

每日记录文件使用 `docs/daily/YYYY-MM-DD.md`，内容固定为：

```markdown
# Day N - 主题

## 今日验收目标

## 实际改动
- 文件：
- 接线/CubeMX：

## 测试
- 固件 commit：
- 操作步骤：
- 期望：
- 实际：
- 日志/图片路径：

## 未解决问题

## 明天第一步
```

---

# 第 1 周：项目二 - FreeRTOS 单板多任务

本周使用项目一中已经跑通所有驱动的那块板。先复制工程到 `exercises/freertos-single-board`，项目一原工程保留为裸机回退基线。

## Day 1：建立 FreeRTOS 最小工程

### 今日结果

FreeRTOS 调度器启动，两个 LED/串口测试任务以不同周期运行；可确认任务切换正常。

### 执行步骤

1. 复制项目一工程，工程名改为 `freertos-single-board`；编译并烧录一次，确认复制后功能仍正常。
2. 在 CubeMX 中启用 FreeRTOS，内存管理选择 `heap_4`，`configTOTAL_HEAP_SIZE` 初始设为 8 KB。
3. 将 HAL timebase 改为 TIM4，FreeRTOS 使用 SysTick；重新生成代码。
4. 新建 `app_freertos.c/.h`，把应用任务创建集中放进 `App_RTOS_Init()`。
5. 创建 `FastBlinkTask` 和 `SlowPrintTask`：前者每 200 ms 改变 LED，后者每 1000 ms 增加计数并输出日志。
6. 任务函数放在文件作用域，任务循环使用 `vTaskDelayUntil()`。
7. 检查 `xTaskCreate()` 返回值；任一创建失败时点亮错误 LED 并停在错误处理函数。
8. 编译、烧录，记录 60 秒串口输出时间戳和计数。

伪代码：

```c
App_RTOS_Init():
    create FastBlinkTask(priority=2)
    create SlowPrintTask(priority=1)
    assert all tasks created

FastBlinkTask():
    last_wake = now
    forever:
        toggle LED
        delay_until(last_wake, 200 ms)

SlowPrintTask():
    last_wake = now
    forever:
        print boot_tick and counter
        counter++
        delay_until(last_wake, 1000 ms)
```

### 验收与产出

- LED 在 200 ms 周期变化；
- 串口连续输出 60 次，计数无跳变；
- `vTaskStartScheduler()` 后无 HardFault；
- 保存 CubeMX 截图、60 秒日志；
- 提交：`feat(rtos): bring up scheduler and periodic tasks`。

## Day 2：把传感器驱动改造成周期任务

### 今日结果

ADC、BH1750、DS18B20 分别由独立任务更新，任务等待期间 CPU 可以调度其他任务。

### 执行步骤

1. 建立 `app_sensor.c/.h`，定义统一的 `SensorSnapshot_t`。
2. 创建 `AdcTask`，每 100 ms 读取 DMA 最新值，使用 8 点滑动平均，更新 `adc` 和时间戳。
3. 将 BH1750 读取拆成“启动测量”和“读取结果”两个动作：发送测量命令后延时 180 ms，再读取两个字节。
4. 创建 `LightTask`，每 500 ms 进行一次完整测量；延时使用 `vTaskDelay()`，不能在 180 ms 内持续占用 CPU。
5. 将 DS18B20 读取拆成“开始温度转换”和“读取 scratchpad”；开始后延时 750 ms，再读取并校验结果。
6. 创建 `TempTask`，每 1000 ms 更新一次温度。
7. 每个驱动返回 `OK/ERROR`，任务根据结果设置 `valid` 位和错误计数。
8. 暂时由 `MonitorTask` 每秒打印快照，验证三个任务在不同周期更新。

伪代码：

```c
LightTask():
    forever:
        if bh1750_start() == OK:
            delay(180 ms)
            result = bh1750_read()
            update_snapshot(result, valid=true)
        else:
            update_snapshot(valid=false)
            light_error_count++
        delay_until_next_500ms_period()

TempTask():
    forever:
        if ds18b20_start_conversion() == OK:
            delay(750 ms)
            result = ds18b20_read_scratchpad_and_check_crc()
            update_snapshot(result)
        delay_until_next_1000ms_period()
```

### 上板测试

1. 转动电位器 5 次，记录 ADC 从低到高的变化；
2. 遮住和照亮 BH1750，记录光照变化；
3. 手握 DS18B20 约 20 秒，记录温升；
4. 在 BH1750 等待 180 ms、DS18B20 等待 750 ms 时确认 LED 任务仍持续运行。

### 验收与产出

- 3 分钟日志中：ADC 约 10 Hz 更新、光照约 2 Hz 更新、温度约 1 Hz 更新；
- 任一慢传感器等待期间 LED 无明显停顿；
- 保存 `day02_sensor_rates.log`；
- 提交：`feat(rtos): run adc light and temperature tasks`。

## Day 3：使用队列汇聚传感器数据

### 今日结果

传感器任务只生产样本，`DataTask` 通过队列接收并维护统一快照；OLED 从快照读取数据。

### 执行步骤

1. 定义 `SensorMsg_t`，包含 `source`、`value`、`valid`、`tick`。
2. 创建长度为 16 的 `sensorQueue`。
3. `AdcTask/LightTask/TempTask` 不再直接改完整快照，而是在得到新数据时向队列发送消息。
4. 创建 `DataTask`，阻塞等待队列；根据 `source` 更新对应字段。
5. 创建 `snapshotMutex`；`DataTask` 持锁更新单个字段，`UiTask` 持锁复制整个快照，随后立即释放。
6. `UiTask` 在锁外格式化字符串和刷新 OLED。
7. 添加 `sensor_queue_drop` 计数；队列发送失败时计数加 1，不在生产任务中永久阻塞。

伪代码：

```c
SensorTask():
    sample = read_sensor()
    if queue_send(sensorQueue, sample, timeout=0) failed:
        sensor_queue_drop++

DataTask():
    forever:
        msg = queue_receive(sensorQueue, wait_forever)
        lock(snapshotMutex)
        update one field in snapshot
        unlock(snapshotMutex)

UiTask():
    every 200 ms:
        lock(snapshotMutex)
        local = snapshot
        unlock(snapshotMutex)
        format local
        refresh OLED
```

### 上板测试

1. 正常运行 5 分钟，确认 `sensor_queue_drop=0`；
2. 临时在 `DataTask` 中加入 1 秒延时，观察队列逐渐填满并出现 drop；
3. 删除故障注入延时，确认恢复后 drop 不再增长；
4. 验证 OLED 每 200 ms 刷新且显示的各字段能独立变化。

### 验收与产出

- 正常版本运行 10 分钟，队列丢弃为 0；
- 故障注入版本能够复现队列满，说明统计有效；
- 保存正常与故障两份日志；
- 提交：`feat(rtos): aggregate sensor samples through queue`。

## Day 4：I2C 互斥锁和按键中断信号量

### 今日结果

BH1750 与 OLED 共享 I2C 时无冲突；按键 EXTI 中断只释放信号量，按键业务在任务中执行。

### 执行步骤

1. 创建 `i2cMutex`，规定 BH1750 和 OLED 所有 I2C 事务必须先获取它。
2. `LightTask` 发送测量命令时获取锁，发送后立即释放；等待 180 ms 时不持锁；读取结果时再次获取锁。
3. `UiTask` 复制数据后获取 `i2cMutex` 刷 OLED，结束后释放。
4. Week 1 练习板的外接按键接 PA2，配置下降沿 EXTI 和内部上拉，避免与 PA0 ADC 冲突。
5. 创建二值信号量 `buttonSem` 和 `ButtonTask`。
6. 在 `HAL_GPIO_EXTI_Callback()` 中调用 `xSemaphoreGiveFromISR()`，按需执行 `portYIELD_FROM_ISR()`。
7. `ButtonTask` 收到信号后延时 20 ms消抖，再读取引脚；仍为按下状态才切换 OLED 页面并计数。

伪代码：

```c
EXTI_Callback(pin):
    if pin == BUTTON_PIN:
        wake = false
        give_from_isr(buttonSem, &wake)
        yield_from_isr_if_needed(wake)

ButtonTask():
    forever:
        take(buttonSem, wait_forever)
        delay(20 ms)
        if button_is_still_pressed():
            page = next_page(page)
            button_press_count++
```

### 上板测试

1. 连续按键 20 次，每次间隔约 1 秒，页面应切换 20 次；
2. 快速抖动按键，记录原始 EXTI 次数和有效按键次数；
3. 同时持续改变光照，运行 10 分钟，I2C 错误计数应为 0；
4. 临时在 OLED 刷新前持锁 300 ms，确认 LightTask 等锁但系统不死锁；随后删除注入代码。

### 验收与产出

- 有效按键次数与人工按键次数一致；
- 正常版本无 I2C 并发错误；
- ISR 中没有打印、OLED 操作或传感器读取；
- 提交：`feat(rtos): protect i2c and handle button via semaphore`。

## Day 5：资源测量和本周验收

### 今日结果

单板 RTOS 练习形成可回退版本，并有栈、堆、队列和 2 小时运行记录。

### 执行步骤

1. 启用 `configCHECK_FOR_STACK_OVERFLOW=2`、`vApplicationStackOverflowHook()` 和 `vApplicationMallocFailedHook()`。
2. `MonitorTask` 每 5 秒输出：每个任务栈高水位、`xPortGetMinimumEverFreeHeapSize()`、队列当前长度、队列历史峰值和丢弃数。
3. 为所有任务建立句柄数组，统一遍历打印水位。
4. Clean/Rebuild，保存 Keil 的 Code/RO-data/RW-data/ZI-data 数值和 map 文件。
5. 启动 2 小时运行；开始、中间、结束各操作一次 ADC、光照、温度和按键。
6. 结束后填写 `docs/resource_report_week1.md`，记录最小堆和每个任务最小剩余栈。
7. 修复本周遗留问题，打标签 `v0.2-freertos-single-board`。

### 本周验收

- [ ] 四类输入均能更新；
- [ ] OLED、BH1750 共用 I2C 连续 2 小时无死锁；
- [ ] 按键由 EXTI -> 信号量 -> 任务处理；
- [ ] 所有任务栈高水位大于 40 words；
- [ ] 最小剩余堆大于 1 KB；
- [ ] 正常负载下所有队列丢弃为 0；
- [ ] 有完整 2 小时日志和资源表。

提交：`test(rtos): complete two-hour single-board validation`。

---

# 第 2 周：项目三 - CAN 数据链路

本周先用最小裸机程序验证 CAN，不带 FreeRTOS 和传感器业务。每通过一层才增加节点数量。

## Day 6：确认硬件并完成单板 Loopback

### 今日结果

一块板在 Internal Loopback 模式连续自发自收 1,000 帧，数据逐字节一致。

### 执行步骤

1. 建立 `exercises/can-loopback`，启用 CAN PB8/PB9 remap、UART 和 LED。
2. 在 CubeMX 配置 Prescaler=4、BS1=15 TQ、BS2=2 TQ、SJW=1 TQ、Mode=Loopback。
3. 配置一个标准帧全通过过滤器到 FIFO0。
4. 按顺序执行 `HAL_CAN_ConfigFilter()`、`HAL_CAN_Start()`。
5. 先用轮询方式发送 ID `0x321`、DLC 8；数据为固定头 `A5 5A`、16 位计数和 4 字节测试模式。
6. 等待 FIFO0 有数据后读取，检查 ID、DLC 和 8 字节内容。
7. 每 100 帧输出一次 `tx_count/rx_count/mismatch_count/error_code`。
8. 断电检查三个收发器模块的型号、VCC、GND、TXD、RXD、CAN_H、CAN_L 标识，画入 `docs/hardware_wiring.md`。

伪代码：

```c
for sequence in 0..999:
    tx = build_test_frame(0x321, sequence)
    assert can_send(tx) == OK
    rx = can_poll_receive(timeout=100ms)
    if rx.id != tx.id or rx.dlc != 8 or rx.data != tx.data:
        mismatch_count++
```

### 验收与产出

- `tx_count=1000`、`rx_count=1000`、`mismatch_count=0`；
- 计算过程和 CubeMX 位时序截图写入 `docs/can_bitrate.md`；
- 硬件接线图完成；
- 提交：`feat(can): pass 1000-frame internal loopback`。

## Day 7：双板 Normal 模式双向通信

### 今日结果

Node A 每 100 ms 发送请求，Node B 收到后回复；两块板各自计数正确。

### 执行步骤

1. 从 Loopback 工程复制出 `can-two-node/node_a` 和 `node_b`，模式改为 Normal。
2. 总线只连接 Node A 和 Node B，两端各接 120 Ω，三线连接 CAN_H、CAN_L、GND。
3. 断电测 CAN_H-CAN_L 电阻并把实测值写入记录；上电后再烧录。
4. Node A 每 100 ms 发送 ID `0x301`，数据包含递增序号；Node B 接收后发送 ID `0x302`，回显相同序号。
5. 两端先使用轮询接收，确认基础链路，再改为 FIFO0 中断接收。
6. Node A 收到回复后计算 `reply_count` 和 `timeout_count`；1 秒未收到相应序号记一次超时。
7. 先运行 10 分钟，再交换两块板的固件角色运行 10 分钟，排除单块板或单个收发器问题。

### 验收与产出

- 每个方向至少 6,000 帧；
- 回复序号与请求一致，`timeout_count=0`；
- 交换板卡角色后结果一致；
- 保存两端日志和总线照片；
- 提交：`feat(can): establish two-node normal-mode exchange`。

## Day 8：中断接收、精确过滤和错误统计

### 今日结果

两块板通过中断接收，Node A 只接收 `0x302`，Node B 只接收 `0x301`，并输出 CAN 错误状态。

### 执行步骤

1. 在 CubeMX 打开 CAN RX0 中断。
2. `HAL_CAN_Start()` 后调用 `HAL_CAN_ActivateNotification(CAN_IT_RX_FIFO0_MSG_PENDING | CAN_IT_ERROR | CAN_IT_BUSOFF)`。
3. Node A 配置精确匹配 `0x302` 的过滤器；Node B 精确匹配 `0x301`。
4. 在 `HAL_CAN_RxFifo0MsgPendingCallback()` 中只读取一帧、保存到固定缓冲并置位；裸机练习可用环形缓冲，不在回调中 `printf`。
5. 主循环消费缓冲并验证序号。
6. 实现 `HAL_CAN_ErrorCallback()`，记录 `HAL_CAN_GetError()`、发送失败数、FIFO overrun 和 bus-off 次数。
7. 使用 Node A 临时发送 ID `0x555`；Node B 的接收计数不应变化。
8. 恢复正常 ID，运行 30 分钟。

回调伪代码：

```c
RxCallback():
    if fifo_has_frame:
        read header and data into ring_buffer[next]
        advance write_index
        rx_irq_count++

main_loop():
    while ring_buffer not empty:
        frame = pop()
        validate(frame)
```

### 验收与产出

- 正常 ID 可接收，`0x555` 被硬件过滤；
- 30 分钟内请求/回复计数一致；
- 错误回调和错误码能在日志中显示；
- 提交：`feat(can): add rx interrupt filters and error counters`。

## Day 9：扩展为三节点总线

### 今日结果

Master 同时接收 Slave 1 的 `0x180` 与 Slave 2 的 `0x181`，并分别向两者发送命令帧。

### 执行步骤

1. 三块板按线型总线连接，只在两个物理端点保留 120 Ω；断电测量约 60 Ω。
2. Master 使用列表/掩码过滤接收 `0x180`、`0x181`、`0x080`、`0x081`。
3. Slave 1 只接收 `0x100`，每 500 ms 发送 `0x180`。
4. Slave 2 只接收 `0x101`，每 500 ms 发送 `0x181`。
5. Master 每 5 秒交替向两个从节点发送一次查询；从节点收到后立即额外发送一帧遥测。
6. Master 分别维护 `s1_rx_count`、`s2_rx_count`、`s1_last_seq`、`s2_last_seq`。
7. 连续运行 1 小时；第 20 分钟拔掉 Slave 1，第 21 分钟重新接入；第 40 分钟对 Slave 2 做同样操作。

### 验收与产出

- 两类周期帧均约 2 Hz；
- 查询后 200 ms 内看到对应节点额外回复；
- 拔掉一个从节点时另一节点仍持续通信；
- 重连节点后自动恢复接收；
- 提交：`feat(can): bring up three-node bus with final ids`。

## Day 10：数据链路长测和冻结

### 今日结果

裸机三节点链路通过 10,000 帧级别测试，形成项目四可复用的 CAN BSP。

### 执行步骤

1. 把过滤器、启动、发送、接收和错误读取封装为 `bsp_can.c/.h`。
2. `bsp_can` 对上层只暴露：`CAN_BSP_Init()`、`CAN_BSP_Send()`、`CAN_BSP_GetErrorStats()`。
3. 将 Slave 1、Slave 2 周期改为 100 ms，运行 10 分钟，Master 理论接收约 12,000 帧。
4. 比较理论帧数、实际帧数和序列号跳变数。
5. 恢复 500 ms 正式周期，Clean/Rebuild 三个工程。
6. 编写 `tests/can_link_test.md`，记录硬件、固件 commit、时长、计数和异常。
7. 打标签 `v0.3-can-link`。

### 本周验收

- [ ] Loopback 1,000/1,000；
- [ ] 双板双向各 6,000 帧无序号错误；
- [ ] 三节点 100 ms 压力测试达到约 12,000 帧；
- [ ] 过滤器拒绝非目标 ID；
- [ ] 单节点掉线不影响其他节点；
- [ ] `bsp_can` 可被后续工程直接复用。

提交：`test(can): complete three-node link validation`。

---

# 第 3 周：项目四 - FreeRTOS 与 CAN 集成

本周建立最终 `firmware/master`、`firmware/slave1`、`firmware/slave2` 三个工程。项目一的驱动、项目二的 RTOS 模式、项目三的 CAN BSP 在这里汇合。

## Day 11：实现公共协议模块

### 今日结果

`can_protocol.c/.h` 可以编码和解码三类帧，并通过固定测试向量。

### 执行步骤

1. 在 `firmware/common` 定义协议版本、ID、命令码、告警码、状态位和错误枚举。
2. 实现 `put_u16_be/get_u16_be/put_s16_be/get_s16_be`。
3. 实现 `Protocol_EncodeSlave1Telemetry()`、`Protocol_EncodeSlave2Telemetry()`、`Protocol_DecodeTelemetry()`。
4. 实现 `Protocol_EncodeCommand()`、`Protocol_DecodeCommand()`。
5. 实现 `Protocol_EncodeAlarm()`、`Protocol_DecodeAlarm()`。
6. 解码顺序固定为：IDE -> RTR -> ID -> DLC -> version -> 字段范围。
7. 为每类帧写至少 3 个有效向量和 5 个无效向量，放入 `tests/protocol_vectors.md`。
8. 在单板测试入口逐个运行测试向量，通过 UART 输出 PASS/FAIL。

协议伪代码：

```c
decode_telemetry(frame, out):
    if frame.ide != STANDARD: return BAD_IDE
    if frame.rtr != DATA: return BAD_RTR
    if frame.id not in [0x180, 0x181]: return BAD_ID
    if frame.dlc != 8: return BAD_DLC
    if frame.data[0] != VERSION_1: return BAD_VERSION
    parse fields using big-endian helpers
    if adc > 4095: return BAD_RANGE
    return OK
```

### 验收与产出

- 所有有效向量解码结果与预期一致；
- 错误 DLC、版本、ID、ADC 范围均被拒绝；
- 负温度测试向量解码正确；
- `can_protocol.c` 不包含 HAL 或 FreeRTOS 头文件；
- 提交：`feat(protocol): implement portable can frame codec`。

## Day 12：完成 Master 的 CAN RX 链路

### 今日结果

Master 实现 `CAN RX ISR -> canRxQueue -> CanRxTask -> MasterSnapshot`，能接收两个裸机从节点的数据。

### 执行步骤

1. 建立 `firmware/master` CubeMX 工程，先启用 UART、CAN、OLED I2C、DS18B20 GPIO、按键 EXTI 和 FreeRTOS。
2. 移入 Master 需要的项目一驱动和项目三 `bsp_can`。
3. 创建长度 16 的 `canRxQueue` 和 `CanRxTask`。
4. 在 CAN RX 回调中从 FIFO0 读出 header/data，填入局部 `CanFrame_t`，调用 `xQueueSendFromISR()`。
5. 队列满时只增加 `can_rx_queue_drop`，不覆盖内存，不在 ISR 输出日志。
6. `CanRxTask` 阻塞接收，调用公共协议解码器；有效遥测更新对应快照和 `last_valid_tick`。
7. 无效帧按错误类型分别累计 `bad_id/bad_dlc/bad_version/bad_range`。
8. `MonitorTask` 每 5 秒打印两个节点的值、序号、帧数和错误计数。

ISR 伪代码：

```c
HAL_CAN_RxFifo0MsgPendingCallback(hcan):
    frame = read_fifo0()
    frame.rx_tick = xTaskGetTickCountFromISR()
    if send_from_isr(canRxQueue, frame) failed:
        stats.rx_queue_drop++
    portYIELD_FROM_ISR(woken)
```

### 验收与产出

- 两个裸机从节点运行时，Master 每 5 秒打印约各 10 帧；
- 协议错误计数为 0；
- 断点检查 ISR 内没有协议解析、OLED 和 UART 调用；
- 提交：`feat(master): receive and decode can frames through rtos queue`。

## Day 13：完成 Slave 1 的采集与发送链路

### 今日结果

Slave 1 实现 `ADC/BH1750 -> snapshot -> TelemetryTask -> canTxQueue -> CanTxTask`。

### 执行步骤

1. 建立 `firmware/slave1`，启用 UART、ADC DMA、I2C、CAN 和 FreeRTOS。
2. 移入项目一的 ADC/BH1750 驱动、项目三 CAN BSP 和公共协议模块。
3. 创建 `AdcTask`、`LightTask`、`TelemetryTask`、`CanTxTask`、`CanRxTask` 和 `MonitorTask`。
4. `AdcTask/LightTask` 更新 Slave 1 快照；锁只保护字段复制。
5. `TelemetryTask` 每 500 ms 复制快照，增加 `telemetry_seq`，编码 `0x180` 并放入 `canTxQueue`。
6. `CanTxTask` 是唯一调用 HAL CAN 发送 API 的任务；邮箱忙时等待短时间重试 3 次，仍失败则计数并丢弃本帧。
7. `CanRxTask` 暂时只识别 `0x100` 查询；收到有效查询后更新 `last_cmd_seq` 并通知 `TelemetryTask` 立即发送。
8. Master 与 Slave 1 两节点运行 30 分钟。

发送伪代码：

```c
TelemetryTask():
    wait until 500ms timeout or query notification
    local = copy_sensor_snapshot()
    frame = encode_slave1(local, seq++, last_cmd_seq)
    if queue_send(canTxQueue, frame, 20ms) failed:
        stats.tx_queue_drop++

CanTxTask():
    forever:
        frame = queue_receive(canTxQueue)
        for attempt in 1..3:
            if CAN_BSP_Send(frame) == OK: break
            delay(2ms)
        update tx_ok or tx_fail
```

### 验收与产出

- Master 30 分钟收到约 3,600 帧 `0x180`；
- 转动电位器和改变光照，Master 解码值同步变化；
- 查询命令后 Slave 1 额外发送一帧且 `last_cmd_seq` 匹配；
- 提交：`feat(slave1): publish adc and light telemetry through tx task`。

## Day 14：完成 Slave 2 的采集、发送和告警状态机骨架

### 今日结果

Slave 2 周期上报 ADC 与温度，查询立即回复；告警状态机已有接口但暂不做最终阈值测试。

### 执行步骤

1. 建立 `firmware/slave2`，启用 UART、ADC DMA、DS18B20 GPIO、CAN 和 FreeRTOS。
2. 移入 ADC、DS18B20、DWT、CAN BSP 和公共协议模块。
3. 创建 `AdcTask`、`TempTask`、`TelemetryTask`、`CanTxTask`、`CanRxTask`、`MonitorTask`。
4. `TempTask` 采用启动转换 -> 延时 750 ms -> 读 scratchpad 的分阶段流程。
5. 编码 `0x181`；负温度按 `int16_t` 大端传输。
6. 接收 `0x101` 查询并立即回复，回显 `last_cmd_seq`。
7. 建立 `Alarm_Update(temperature_x10, valid)` 接口，返回 `NO_CHANGE/ACTIVATED/CLEARED/SENSOR_FAULT`，本日先用人工输入测试接口。
8. Master 与 Slave 2 两节点运行 30 分钟。

### 验收与产出

- Master 30 分钟稳定收到 `0x181`；
- ADC、温度、状态位解码正确；
- 查询序号闭环正确；
- 传感器拔掉时 `temperature_valid` 清零并计数；
- 提交：`feat(slave2): publish adc and temperature telemetry`。

## Day 15：三节点 RTOS 基础联调

### 今日结果

三节点全部运行 FreeRTOS，周期遥测和查询链路连续运行 1 小时。

### 执行步骤

1. 依次烧录 Master、Slave 1、Slave 2 的当天最新固件，并在日志首行打印角色、版本和 Git 短 hash。
2. 三节点上电后记录前 2 分钟日志，确认每个节点初始化返回值全部为 OK。
3. 操作 Slave 1 的电位器/光照和 Slave 2 的电位器/温度，确认 Master 输出对应变化。
4. 每 5 分钟由 Master 查询一次两个节点，记录命令序号和回复延迟。
5. 连续运行 1 小时，统计每个从节点理论帧数 7,200 左右与实际接收数。
6. 若出现序号跳变，保存发生前后至少 20 行日志，定位是 TX 队列、CAN 发送还是 RX 队列丢失。
7. 修复后重新从 0 开始完成 1 小时测试。
8. 打标签 `v0.4-three-node-rtos-telemetry`。

### 本周验收

- [ ] 三节点均为 FreeRTOS 固件；
- [ ] 两个从节点 500 ms 周期稳定；
- [ ] Master 的 ISR 入队、任务解码链路完整；
- [ ] 两个从节点的单一 TX Task 链路完整；
- [ ] 两类查询命令均能立即回复；
- [ ] 1 小时正常运行无 HardFault、无队列丢弃。

提交：`test(system): validate one-hour three-node rtos telemetry`。

---

# 第 4 周：项目五 - 完整业务闭环

## Day 16：完成 Master 本地温度与 OLED 页面

### 今日结果

OLED 能在两个页面显示本地温度、两个从节点数据、在线状态和告警状态。

### 执行步骤

1. 将 Master 的 DS18B20 驱动接入 `LocalTempTask`，每秒更新本地温度和有效位。
2. 定义页面 0：Slave 1 ADC/光照；页面 1：Slave 2 ADC/温度；每页同时显示节点 `ON/OFF` 和告警标志。
3. `UiTask` 每 200 ms 锁内复制 `MasterSnapshot_t`，锁外生成显示字符串。
4. 只在显示内容发生变化或满 1 秒时刷新 OLED，记录 `ui_refresh_count`。
5. 温度使用整数拆分显示：`value/10` 和 `abs(value%10)`，避免浮点 `printf`。
6. 单独运行 Master 10 分钟，再接入两个从节点运行 20 分钟。

### 验收与产出

- 页面字段与串口解码值一致；
- 切换页面不影响 CAN 接收计数；
- 拔掉 Master 本地 DS18B20 时显示 `ERR`，系统其余功能继续运行；
- 保存两页清晰照片；
- 提交：`feat(master): display local and remote snapshots on oled`。

## Day 17：完成按键查询与命令确认

### 今日结果

Master 按键可选择节点并发送查询，OLED/日志显示查询序号、回复结果和耗时。

### 执行步骤

1. Master 按键最终接 PA0，配置 EXTI；ISR 释放 `buttonSem`。
2. 短按在 Slave 1 和 Slave 2 之间切换查询目标；本项目只需一个按键时，可采用奇偶次数交替目标。
3. `ButtonTask` 消抖后生成命令帧，写入 `cmd_seq`，记录 `send_tick`，放入 `canTxQueue`。
4. 从节点 `CanRxTask` 校验命令后保存 `last_cmd_seq`，通知 `TelemetryTask` 立即上报。
5. Master 收到遥测时比较 `last_cmd_seq`；匹配则计算 `ack_latency_ms` 并将 pending 状态清除。
6. 500 ms 内未匹配则记录 `cmd_timeout`；重复/旧序号只计数，不误判为当前回复。
7. 对两个节点各执行 20 次查询，记录成功数、超时数、最小/最大/平均耗时。

状态伪代码：

```c
on_button(target):
    cmd_seq++
    pending[target] = {seq=cmd_seq, send_tick=now, active=true}
    enqueue command

on_telemetry(node, last_cmd_seq):
    if pending[node].active and last_cmd_seq == pending[node].seq:
        latency = now - pending[node].send_tick
        pending[node].active = false
        stats.query_ok++

periodic_check():
    if pending active for > 500ms:
        pending = false
        stats.query_timeout++
```

### 验收与产出

- 两个节点各 20 次查询全部得到匹配序号回复；
- 日志包含目标、命令序号、回复序号和耗时；
- 结果写入 `tests/query_test.csv`；
- 提交：`feat(system): close query command acknowledgement loop`。

## Day 18：完成在线、离线和恢复状态机

### 今日结果

Master 能在 3 秒无有效遥测后判定节点离线，恢复收到有效帧后只产生一次恢复事件。

### 执行步骤

1. 每个节点保存 `last_valid_rx_tick`、`online`、`offline_count`、`recovery_count`。
2. `CanRxTask` 只有在协议帧全部校验通过后才更新 `last_valid_rx_tick`。
3. `OnlineTask` 每 200 ms 检查 `now - last_valid_rx_tick > 3000 ms`。
4. 在线 -> 离线时更新状态、记录一次事件并刷新 OLED；持续离线期间不重复刷日志。
5. 离线 -> 在线时由第一帧有效遥测触发恢复，记录离线持续时间。
6. 依次执行：拔 Slave 1 5 秒再接回；拔 Slave 2 10 秒再接回；两者同时断电 5 秒再接回。
7. 对每次测试记录“断开时刻、离线判定时刻、恢复上电时刻、首帧时刻”。

伪代码：

```c
OnlineTask every 200ms:
    for node in nodes:
        if node.online and elapsed(now, node.last_valid_tick) > 3000ms:
            node.online = false
            emit OFFLINE once

on_valid_telemetry(node):
    node.last_valid_tick = now
    if not node.online:
        node.online = true
        emit RECOVERED once
```

### 验收与产出

- 三种断电场景均按 3.0～3.2 秒判离线；
- 重连后的第一批有效帧触发恢复；
- 未断开的节点持续通信；
- 结果写入 `tests/online_recovery_test.csv`；
- 提交：`feat(master): detect node timeout and recovery`。

## Day 19：完成高温告警和传感器故障事件

### 今日结果

Slave 2 使用滞回阈值生成高温激活/解除事件；传感器故障也能产生事件并在遥测状态位体现。

### 执行步骤

1. 正式阈值设置为：温度 `>= 30.0 ℃` 激活，`<= 28.0 ℃` 解除。测试时可临时改为接近室温的值，测试后恢复并记录宏定义。
2. `TempTask` 每次得到新温度后调用告警状态机。
3. 状态从 NORMAL -> ACTIVE 时，置遥测告警位并向 TX 队列放入 `0x081` 激活事件。
4. 状态从 ACTIVE -> NORMAL 时，清告警位并发送解除事件。
5. 阈值中间区间保持当前状态，避免温度抖动导致重复告警。
6. 连续 3 次 DS18B20 读取失败时产生 `SENSOR_FAULT` 激活事件；连续 3 次成功时产生解除事件。
7. Slave 1 对 BH1750 使用同样的连续失败/连续成功状态机，通过 `0x080` 发送传感器故障激活和解除事件。
8. Master 解码 `0x080/0x081` 并更新快照，OLED 显示 `ALM` 或 `SENSOR ERR`，LogTask 输出事件。
9. 用手加热/冷却 Slave 2 传感器完成 3 个高温循环；再分别拔插 BH1750 和 DS18B20，各完成 2 个故障循环。

状态机伪代码：

```c
if temp_valid:
    failure_streak = 0
    if state == NORMAL and temp >= 300:
        state = ACTIVE
        send_alarm(ACTIVATED, temp)
    else if state == ACTIVE and temp <= 280:
        state = NORMAL
        send_alarm(CLEARED, temp)
else:
    failure_streak++
    if failure_streak == 3:
        send_alarm(SENSOR_FAULT, ACTIVATED)
```

### 验收与产出

- 3 个高温循环每个仅产生一次激活和一次解除；
- 28.0～30.0 ℃ 区间不重复切换；
- 两个从节点的传感器故障和恢复均各产生一次事件；
- 提交：`feat(system): add alarm and sensor fault event handling`。

## Day 20：统一结构化日志并完成业务验收

### 今日结果

三个节点的日志格式统一，完整业务功能通过一轮回归，形成可演示版本。

### 执行步骤

1. 定义日志行格式：

```text
timestamp_ms,node,level,event,key1=value1,key2=value2
12500,MASTER,INFO,RX_TELEMETRY,src=S1,seq=24
13120,MASTER,WARN,NODE_OFFLINE,src=S2,last_seen_ms=10010
```

2. 每个任务只向 `logQueue` 发送固定长度消息；`LogTask` 独占 UART。
3. 添加日志事件：BOOT、SENSOR、TX、RX、QUERY、QUERY_ACK、OFFLINE、RECOVERED、ALARM、CAN_ERROR、QUEUE_DROP、MONITOR。
4. 控制日志频率：周期遥测不逐帧打印，只每 5 秒汇总；状态变化立即打印。
5. 按最终功能清单从上电开始完整演示一次，并录像作为内部回归证据。
6. 修复功能缺陷后重新执行整轮，不在失败记录上直接勾选通过。
7. 打标签 `v0.5-feature-complete`。

### 本周验收

- [ ] OLED 两页和本地温度完成；
- [ ] 两节点查询各 20 次通过；
- [ ] 单节点和双节点断电/恢复通过；
- [ ] 高温滞回与传感器故障通过；
- [ ] 日志可由 CSV 方式解析；
- [ ] 完整演示流程能在 3 分钟内完成。

提交：`test(system): complete feature regression`。

---

# 第 5 周：可靠性、故障注入和资源收尾

## Day 21：建立完整运行统计

### 今日结果

每个节点可以回答“发了多少、收了多少、错在哪里、队列最高多深、任务还剩多少栈”。

### 执行步骤

1. 定义 `RuntimeStats_t`，至少包含：`tx_ok`、`tx_fail`、`rx_ok`、`bad_id`、`bad_dlc`、`bad_version`、`bad_range`、`seq_gap`、`rx_queue_drop`、`tx_queue_drop`、`can_error`、`bus_off`。
2. 为每个队列维护历史峰值；每次发送/接收后更新 `peak_depth`。
3. 为周期任务维护 `loop_count` 和 `last_progress_tick`。
4. `MonitorTask` 每 5 秒输出统计快照、最小剩余堆和各任务栈高水位。
5. Master 计算每个从节点的序列号间隔：`delta=(uint8_t)(new-old)`；`delta>1` 时将 `delta-1` 累加到 `seq_gap`。
6. 正常运行 1 小时，生成 `tests/baseline_stats.csv`。

### 验收与产出

- 所有统计项能通过正常运行或人工注入发生变化；
- 1 小时基线下队列 drop 和协议错误为 0；
- 理论帧数与实测差异有明确说明；
- 提交：`feat(system): expose runtime and resource statistics`。

## Day 22：执行队列和总线压力测试

### 今日结果

获得 500 ms、100 ms、20 ms 三档发送周期下的处理能力数据和失效点。

### 执行步骤

1. 把从节点发送周期改为编译宏 `TELEMETRY_PERIOD_MS`，正式值为 500。
2. 分别烧录 500 ms、100 ms、20 ms 三档固件，每档运行 20 分钟。
3. 每档记录 Master 的 `rx_ok/seq_gap/rx_queue_peak/rx_queue_drop`，两个从节点的 `tx_ok/tx_fail/tx_queue_peak`。
4. 在 20 ms 档临时让 `CanRxTask` 每帧延时 30 ms，验证 RX 队列满统计；保存证据后删除延时。
5. 恢复正常任务后再次跑 20 ms 档，区分“系统处理能力”与“故障注入造成的处理不足”。
6. 把结果写入 `tests/load_test.csv`，画一张“发送周期 vs 丢弃/队列峰值”表或图。
7. 最终固件恢复 500 ms，并在构建日志中打印该配置。

### 验收与产出

- 三档测试都有固件 hash、时长和计数；
- 明确记录首次出现丢弃的测试条件；
- 恢复 500 ms 后正常运行 30 分钟无 drop；
- 提交：`test(system): characterize queue behavior under load`。

## Day 23：执行协议和节点故障注入

### 今日结果

无效报文不会更新 Master 在线时间和业务数据；节点断电、传感器断开都能被识别。

### 执行步骤

1. 在 Slave 1 增加仅测试构建启用的 `fault_injection.c`，依次发送错误 DLC、错误 version、ADC=5000、错误 ID 和重复序号。
2. Master 对每种无效帧分别增加相应错误计数，业务快照保持原值。
3. 连续只发送无效帧超过 3 秒，验证 Master 仍将该节点判定为离线。
4. 关闭测试构建，恢复正常帧后验证节点恢复。
5. 依次拔掉 Slave 1 BH1750、Slave 2 DS18B20、Slave 1 CAN_H/L、Slave 2 电源，执行 `tests/fault_injection_matrix.md` 中的用例。
6. 每个用例记录：注入动作、预期计数/状态、实际结果、恢复动作、恢复时间。

### 验收与产出

- 5 种协议错误都被准确分类；
- 无效帧不刷新在线时间；
- 4 种硬件故障都有正确状态和恢复结果；
- 提交：`test(system): validate protocol and hardware fault handling`。

## Day 24：CAN 错误恢复测试

### 今日结果

系统能记录 CAN 错误状态；物理链路恢复后节点可以重新通信，不需要人工复位 MCU。

### 执行步骤

1. CubeMX 中启用 `AutoBusOff`，保留 `AutoRetransmission`；记录最终 `hcan.Init` 配置。
2. 在 CAN error callback 中保存最后错误码、错误次数和 bus-off 次数；将错误事件交给日志任务。
3. 正常运行 5 分钟后断开 Slave 1 的 CAN_H/CAN_L，但保持 MCU 供电并继续发送 10 秒。
4. 记录 Slave 1 的发送失败、错误码和状态；重新接线，记录恢复首帧时间。
5. 对 Slave 2 重复一次。
6. 如果确实进入 bus-off，记录进入和自动恢复时间；如果实验条件只产生 ACK/error warning，则按实际状态记录，不把它写成 bus-off 测试通过。
7. 恢复连接后运行 30 分钟，确认错误计数不再增长、周期遥测恢复。

### 验收与产出

- 断线期间有明确错误码和发送失败计数；
- 重新接线后 5 秒内恢复有效遥测；
- 结果中区分“观察到的错误状态”和“未触发的 bus-off”；
- 提交：`test(can): verify disconnect errors and link recovery`。

## Day 25：看门狗、内存和本周回归

### 今日结果

看门狗只在关键任务都持续推进时刷新；三节点资源有实测报告，正式固件通过 2 小时回归。

### 执行步骤

1. 为关键任务定义进度位或进度计数：Master 的 `CanRxTask/CanTxTask/OnlineTask`，从节点的 `SensorTask/CanTxTask`。
2. 创建 `HealthTask` 每 1 秒比较本轮与上轮进度；所有必需任务均前进才刷新 IWDG。
3. IWDG 超时时间先设为约 4 秒；启动时读取复位原因并记录 `WATCHDOG_RESET`。
4. 故障测试：临时让一个关键任务进入死循环且允许中断，确认看门狗复位；删除注入后验证正常运行不复位。
5. Clean/Rebuild 三节点，保存 map 文件和 Keil 内存摘要。
6. 汇总各任务栈高水位；若某任务剩余小于 40 words，先增加；若远大于需求，可在保留安全余量后缩减。
7. 使用正式 500 ms 固件完整回归 2 小时，执行一次查询、一次离线恢复和一次告警。
8. 打标签 `v0.6-core-complete`。

看门狗伪代码：

```c
HealthTask every 1s:
    healthy = true
    for each critical task:
        if current_progress == previous_progress:
            healthy = false
        previous_progress = current_progress
    if healthy:
        refresh_iwdg()
    else:
        log HEALTH_STALL once
```

### 本周验收

- [ ] 运行统计和资源水位齐全；
- [ ] 三档压力测试完成；
- [ ] 协议、传感器、节点断电故障注入完成；
- [ ] CAN 断线与恢复有记录；
- [ ] 看门狗故障注入能触发复位，正常版本不复位；
- [ ] 正式版本 2 小时回归通过。

提交：`test(system): complete reliability and resource regression`。

---

# 第 6 周：自动化测试、24 小时长测和项目交付

## Day 26：建立串口采集和日志分析工具

### 今日结果

PC 可以自动保存串口日志，脚本能统计事件数、错误数、序列号跳变和在线恢复时间。

### 执行步骤

1. 在 `tools/requirements.txt` 写入 `pyserial`；如需画图再加入 `pandas/matplotlib`。
2. 实现 `serial_logger.py`：命令行参数为串口号、波特率、输出文件；每行附加 PC 时间并立即 flush。
3. 实现 `analyze_log.py`：解析统一 CSV 日志，按节点统计 BOOT、TX、RX、ERROR、QUEUE_DROP、OFFLINE、RECOVERED、ALARM。
4. 对序列号计算重复和 gap；对 OFFLINE/RECOVERED 配对计算离线持续时间。
5. 给脚本准备 20～30 行固定样例日志，手工算出期望结果并对照脚本输出。
6. 使用真实串口采集 30 分钟，生成 `artifacts/logs/day26_real.log` 和分析摘要。

命令形式：

```powershell
python tools/serial_logger.py --port COM5 --baud 115200 --out artifacts/logs/master.log
python tools/analyze_log.py artifacts/logs/master.log --summary artifacts/logs/master_summary.csv
```

### 验收与产出

- 串口断开时工具给出明确错误并保留已写数据；
- 固定样例的统计结果与手算一致；
- 30 分钟真实日志可完整解析；
- 提交：`feat(tools): capture and analyze structured serial logs`。

## Day 27：执行 4 小时预长测并修复

### 今日结果

正式固件和采集脚本连续运行 4 小时，为次日 24 小时测试排除明显问题。

### 执行步骤

1. 固定三节点 commit，烧录后记录固件 hash、供电方式、串口号和起始计数。
2. Master 串口接入 `serial_logger.py`，开始 4 小时采集。
3. 第 1 小时执行查询；第 2 小时断开 Slave 1 5 秒；第 3 小时触发一次告警；第 4 小时不操作。
4. 运行结束后执行 `analyze_log.py`。
5. 检查：是否复位、序列号 gap、队列 drop、CAN 错误、堆最低值、任务栈最低值、离线恢复时间。
6. 修复发现的问题；每个修复都重新执行对应的 30 分钟定向测试。
7. 将结果写入 `tests/pre_soak_4h.md`。

### 验收与产出

- 4 小时无意外复位和死锁；
- 正常区间无队列丢弃；
- 三次操作事件在日志中可定位；
- 所有修复有单独 commit 和复测记录；
- 提交：`test(system): pass four-hour pre-soak`。

## Day 28：启动并守护 24 小时稳定性测试

### 今日结果

24 小时测试开始运行，测试环境和操作时间表完整记录。

### 执行步骤

1. 使用 Day 27 通过的唯一 commit，Clean/Rebuild 并烧录三块板。
2. 在 `tests/soak_24h.md` 写入开始时间、三个固件 hash、接线照片、电源方式、室温、发送周期和测试人员。
3. 启动串口采集；确认前 5 分钟日志和磁盘文件持续增长。
4. 建议 09:00 启动；11:00 查询两个节点，15:00 让 Slave 1 断电 5 秒，19:00 触发温度告警，次日 08:00 让 Slave 2 断电 5 秒，次日 09:00 再查询两个节点并结束测试。
5. 每次操作只记录准确时间和动作，不改固件、不重新编译。
6. 测试期间若系统失败，保存现场日志和状态，以失败时间为结束点；修复后重新开始一轮完整 24 小时测试。

### 当天产出

- `artifacts/logs/soak_24h_master.log`；
- `tests/soak_24h.md` 环境与操作记录；
- 测试开始后前 30 分钟检查结果。

本日提交只包含测试记录：`test(system): start 24-hour soak on release candidate`。

## Day 29：分析长测并形成测试报告

### 今日结果

24 小时数据被整理成可引用的结果，所有数字都能回到原始日志。

### 执行步骤

1. 停止采集，保存结束时间和三节点最终 OLED/UART 状态照片。
2. 运行分析脚本，生成事件汇总和错误汇总。
3. 计算预期周期帧：每节点约 `24*60*60/0.5 = 172,800` 帧；查询额外帧单独计入。
4. 记录实收帧数、序号 gap、重复帧、队列 drop、CAN 发送失败、复位次数、最小堆和最小栈。
5. 检查四个预定操作的离线判定、恢复时间、告警激活/解除和查询耗时。
6. 完成 `tests/final_test_report.md`，包含测试环境、用例表、结果、日志路径、限制和结论。
7. 把关键实测数据同步到 `docs/resource_report.md` 和 README 草稿。

测试结果表：

| 指标 | Master | Slave 1 | Slave 2 | 证据文件 |
|---|---:|---:|---:|---|
| 运行时长 |  |  |  |  |
| 有效接收/发送帧 |  |  |  |  |
| 序列号 gap |  |  |  |  |
| RX/TX 队列丢弃 |  |  |  |  |
| CAN 错误 |  |  |  |  |
| 意外复位 |  |  |  |  |
| 最小剩余堆 |  |  |  |  |
| 最小任务栈 |  |  |  |  |
| 离线判定时间 |  |  |  |  |
| 恢复时间 |  |  |  |  |

### 验收与产出

- 报告中的每个数字都有日志或 map 文件来源；
- 24 小时内的预定故障操作全部能从日志中复原；
- 若有异常，报告写明现象、影响和后续处理，不删除失败数据；
- 提交：`docs(test): publish 24-hour stability report`。

## Day 30：完成 README、演示视频和 v1.0.0

### 今日结果

项目达到可公开展示、可复现、可用于简历投递的状态。

### 执行步骤

1. README 按以下顺序完成：项目一句话、演示图、硬件清单、系统架构、节点职责、任务表、协议表、接线、环境、编译烧录、运行效果、测试数据、已知限制、目录导航。
2. `docs/architecture.md` 画出 ISR、队列、任务和数据快照链路。
3. `docs/can_protocol.md` 放完整 ID 与字节表，并链接测试向量。
4. 将三个 `.hex` 或 `.bin` 放入 `artifacts/firmware/v1.0.0/`，文件名带节点和版本。
5. 录制 2～3 分钟演示：系统全景 -> 实时数据 -> 查询 -> 节点离线恢复 -> 温度告警 -> 测试结果。
6. 从一台未打开过工程的环境，按 README 执行一次 Clean/Rebuild；至少确认三工程都能编译。
7. 检查仓库中没有临时绝对路径、无用大文件、密钥、个人串口配置和失败构建产物。
8. 更新 CHANGELOG，打标签 `v1.0.0`。

### 本周验收

- [ ] 自动采集和分析脚本可运行；
- [ ] 4 小时预长测通过；
- [ ] 完整 24 小时测试完成；
- [ ] 最终测试报告完成；
- [ ] README 可指导编译、烧录、接线和运行；
- [ ] 三个发布固件、演示视频和架构图齐全；
- [ ] `v1.0.0` 可回退复现。

提交：`release: publish can distributed system v1.0.0`。

---

# 第 7 周：项目总结、简历和面试

这一周不再加业务功能。目标是把已经完成的工程转化为能被面试官快速理解、继续追问且有证据支撑的项目材料。

## Day 31：写完整项目总结

### 今日结果

完成 `docs/project_summary.md`，能从需求、架构、实现、测试、问题和结果六个方面讲清整个项目。

### 执行步骤

1. 从测试报告提取真实数字，填写项目数据卡：

```text
节点数：3
CAN 速率：500 kbit/s
正式遥测周期：500 ms
长测时长：____ h
Master 有效接收：____ 帧
序列号 gap：____
队列丢弃：____
最小剩余堆：____ bytes
最小任务栈水位：____ words
节点离线判定：____ ms
节点恢复：____ ms
查询响应：min/avg/max = ____/____/____ ms
```

2. 用 300～500 字写“为什么做”：三节点采集、CAN 通信、RTOS 并发、可靠性观测。
3. 用一张表写“我负责什么”：硬件接线、外设驱动复用、协议、RTOS 架构、测试脚本、报告。
4. 用一张数据流图解释 `RX ISR -> RX Queue -> CanRxTask -> Snapshot -> UiTask`。
5. 写三项设计选择及理由：固定 8 字节协议、单一 CAN TX Task、慢传感器分阶段读取。
6. 写项目限制：实验级硬件、未做 CAN FD/UDS、未做功能安全、bus-off 是否真实触发以测试结果为准。
7. 最后一节写“下一步可扩展”，只列 Bootloader、USB-CAN 自动化或 DBC 中的一到两项。

### 验收与产出

- 总结中的数字全部来自 Day 29 报告；
- 任何设计结论都能指向一个源文件或测试；
- 自己朗读一遍控制在 8 分钟内；
- 提交：`docs: summarize architecture implementation and measured results`。

## Day 32：整理两个真实 Bug 复盘

### 今日结果

完成两篇可在面试中讲 3～5 分钟的 Bug 复盘，每篇都有现象、假设、测量、根因、修复和复测。

### 执行步骤

1. 从 35 天记录中选择两个证据最完整的问题，优先选择不同层次：一个 CAN/硬件问题，一个 RTOS/并发问题。
2. 收集当时日志、错误码、接线照片、相关 commit diff 和复测结果。
3. 按以下结构写入 `docs/bugs/bug-01.md` 和 `bug-02.md`：

```markdown
# 问题标题

## 现象与复现条件
哪个版本、哪个节点、执行什么后出现什么。

## 初始假设
按可能性列出 2～4 个假设。

## 定位过程
每一步测了什么，排除了什么，下一步为什么这样做。

## 根因
说明代码、电气或时序机制，不只写“配置错了”。

## 修复
具体文件、配置或接线改动。

## 复测
相同用例、长测或压力测试的实际结果。

## 防止再次发生
新增了什么计数、断言、测试或文档。
```

4. 为每个 Bug 写一个 90 秒口述版，录音并检查是否讲出了证据链。
5. 准备追问：为什么最初没发现、为什么该修复有效、有没有副作用、怎样自动化防复发。

### 验收与产出

- 两个 Bug 都来自真实开发记录；
- 每篇至少引用一份日志和一个 commit；
- 90 秒口述不依赖看稿；
- 提交：`docs: add two evidence-based bug postmortems`。

## Day 33：准备 CAN 专项面试

### 今日结果

完成 CAN 问题卡片，并能结合自己的位时序、过滤器、错误记录和帧格式回答。

### 执行步骤

1. 对以下问题各写“结论一句 + 原理两三句 + 本项目证据一句”：

| 问题 | 回答时必须关联的项目证据 |
|---|---|
| 为什么需要两个 120 Ω 终端 | 接线图和断电约 60 Ω 实测 |
| 为什么使用线型而不是星型 | 三节点总线照片与支线长度 |
| 500 kbit/s 怎么算 | 36 MHz、4、15、2 的计算 |
| 标准帧和扩展帧区别 | 本项目 IDE 校验与 11 位 ID |
| 仲裁为什么小 ID 优先 | 告警 `0x080/0x081` 比遥测优先 |
| Loopback 能证明什么 | Day 6 与 Day 7 两种测试边界 |
| Normal 模式为什么需要 ACK | Day 24 断线发送错误 |
| 过滤器如何配置 | Master 与从节点过滤范围 |
| ISR 为什么只搬帧 | `HAL_CAN_Rx...Callback` 代码 |
| DLC/版本/范围为何都校验 | Day 23 故障注入结果 |
| 序列号有什么用 | 长测 gap/重复统计 |
| bus-off 怎么处理 | 实际 AutoBusOff 配置与测试现象 |
| 总线负载如何估算 | 两节点周期、帧频率和 Day 22 数据 |

2. 手写一次遥测帧编码：给定 ADC=2048、温度=25.3 ℃，写出 8 字节结果。
3. 手算一次 500 kbit/s 位时序和采样点。
4. 在代码中定位：过滤器配置、CAN 启动顺序、RX 回调、TX Task、错误回调。
5. 进行 30 分钟自问自答录音，回听后修正含糊处。

### 验收与产出

- 13 个问题均能在 90 秒内回答；
- 位时序和字节编码可脱离资料手算；
- 任何“可靠、实时、无丢包”的表述都能给出对应测试条件和数据。

## Day 34：准备 FreeRTOS、C 和调试专项面试

### 今日结果

能从本项目代码解释队列、互斥锁、信号量、任务通知、中断 API、栈/堆和看门狗。

### 执行步骤

1. 对以下问题制作同样的“结论 + 原理 + 项目证据”卡片：

| 问题 | 项目证据 |
|---|---|
| queue、mutex、binary semaphore 各解决什么 | sensorQueue、snapshotMutex、buttonSem |
| 为什么 ISR 使用 FromISR API | Master RX 与按键 EXTI 回调 |
| 为什么 `portYIELD_FROM_ISR` | 高优先级任务即时获得运行机会 |
| `vTaskDelay` 与 `vTaskDelayUntil` 区别 | 传感器等待与周期任务 |
| 为什么 CAN TX 只有一个任务 | 避免邮箱并发和统一错误处理 |
| 为什么锁内只复制快照 | OLED/I2C 是慢操作 |
| 如何确定栈大小 | Day 25 高水位实测 |
| heap_4 的用途 | 第 1 周和最终最小剩余堆 |
| 队列满如何处理 | drop 计数和 Day 22 压测 |
| 看门狗为什么检查任务进度 | HealthTask 逻辑和故障注入 |
| `volatile` 能否代替锁 | 多字段快照一致性 |
| 为什么不直接发送结构体 | padding、对齐和端序 |
| 如何定位 HardFault | 寄存器、栈、map、任务水位 |
| DS18B20 等 750 ms 时系统为何不卡 | 分阶段转换和任务阻塞 |

2. 画出 Master 的任务优先级表，并解释为什么 RX 高于 UI/Log。
3. 给出一个队列满案例：现象、峰值、drop、根因、处理。
4. 给出一个栈不足案例的排查顺序，即使项目未真实发生，也明确说明这是排查方案而不是项目实测。
5. 在工程中随机抽 5 个函数，口述输入、输出、并发上下文和失败路径。

### 验收与产出

- 能在白纸上重画 RTOS 数据流；
- 能区分项目实测与理论方案；
- 每个回答都能落到具体代码或日志。

## Day 35：简历定稿和两轮模拟面试

### 今日结果

简历项目描述使用真实数据；完成 1 分钟电梯介绍、5 分钟项目介绍和一轮 30 分钟深挖。

### 执行步骤

1. 用 Day 29 的真实数字填写简历，不保留方括号：

```text
基于 STM32F103、FreeRTOS 与 Classic CAN 的三节点分布式采集系统
- 设计一主两从固件与固定 8 字节 CAN 协议，在 500 kbit/s 总线上实现 500 ms 周期遥测、主动查询、3 s 离线检测、自动恢复及温度告警；通过 ID、DLC、版本、端序、范围和序列号校验处理异常帧。
- 构建 CAN RX ISR -> FreeRTOS Queue -> Protocol Task 数据链路，并以独立 TX/Log 任务统一管理 CAN 邮箱与 UART；实测最小剩余堆为 ___ B，关键任务最小栈水位为 ___ words。
- 编写 Python 串口采集与分析工具，完成队列压力、协议错误、节点断电、传感器失效和 ___ 小时稳定性测试；累计接收 ___ 帧，记录队列丢弃 ___、序列号 gap ___，节点恢复时间 ___ ms。
```

2. 准备 1 分钟介绍：目标 10 秒、架构 20 秒、最难点 15 秒、测试结果 15 秒。
3. 准备 5 分钟介绍：背景 30 秒、硬件与协议 60 秒、RTOS 架构 90 秒、关键问题 60 秒、测试与结果 60 秒。
4. 第一轮模拟面试 30 分钟：只讲项目，按 Day 33～34 的问题随机追问。
5. 复盘含糊、过度表述和没有证据的回答，修改项目总结与简历。
6. 第二轮模拟面试 30 分钟：加入 C 语言、嵌入式调试、项目取舍和失败经历。
7. 最后检查所有公开材料中的数字完全一致：简历、README、测试报告、视频字幕、项目总结。

### 面试开场模板

```text
这个项目是一个基于三块 STM32F103 的一主两从分布式采集系统。
两个从节点采集光照、温度和 ADC，通过 500 kbit/s CAN 每 500 ms 上报；
主节点负责 OLED 显示、主动查询、3 秒离线检测和告警展示。

软件上我把 CAN 接收拆成 ISR 入队和任务解析，把所有发送集中到 CAN TX Task，
慢传感器采用分阶段转换，避免阻塞其他任务。协议使用固定 8 字节帧，包含版本、
序列号、状态位和命令回显，并对 ID、DLC、范围等逐层校验。

最后我做了协议错误、队列压力、节点断电和传感器失效测试，并连续运行 ___ 小时。
实测接收 ___ 帧，队列丢弃 ___，节点平均恢复时间 ___ ms。
```

### 遇到不会的追问时的回答方式

```text
这个机制我目前没有在项目里实际实现，所以不能把它说成实测结果。
我现在能确认的是：本项目采用了 ______，日志/测试观察到 ______。
如果要加入您问的功能，我会先从 ______ 验证，再按 ______ 的顺序集成。
```

### 最终验收

- [ ] 简历中的每个数字均来自测试报告；
- [ ] 1 分钟介绍控制在 50～70 秒；
- [ ] 5 分钟介绍能画出架构和协议帧；
- [ ] 两个 Bug 各能讲 90 秒并应对追问；
- [ ] CAN 和 FreeRTOS 问题能引用实际代码；
- [ ] 两轮模拟面试都有录音和复盘记录。

---

## 4. 项目结束时的材料清单

### 4.1 代码与固件

- [ ] Master、Slave 1、Slave 2 均可 Clean/Rebuild；
- [ ] 三个发布固件带版本号；
- [ ] 公共协议模块只有一份定义；
- [ ] 测试注入代码默认关闭；
- [ ] 所有 HAL、FreeRTOS 和队列操作的失败路径有计数；
- [ ] Git tag 包含 `v0.2`、`v0.3`、`v0.4`、`v0.5`、`v0.6`、`v1.0.0`。

### 4.2 工程证据

- [ ] CAN 接线图和实物照片；
- [ ] 500 kbit/s 位时序计算；
- [ ] 协议字节表与测试向量；
- [ ] 三节点任务表和数据流图；
- [ ] 压力测试 CSV；
- [ ] 故障注入矩阵；
- [ ] 24 小时原始日志和分析结果；
- [ ] map 文件、堆和任务栈报告；
- [ ] 两篇真实 Bug 复盘；
- [ ] 2～3 分钟演示视频。

### 4.3 可用于面试的五个核心结论

1. **我完成了什么**：三节点采集、CAN 通信、查询、在线检测、告警和自动恢复；
2. **系统怎样运行**：中断只接收并入队，任务解析协议，独立 TX Task 发送，快照供 UI 和监控读取；
3. **为什么这样设计**：控制 ISR 时长、减少共享外设并发、避免慢传感器阻塞、让错误可统计；
4. **怎样证明它工作**：功能用例、压力测试、故障注入、资源水位和 24 小时日志；
5. **项目边界是什么**：这是实验级 MCU/CAN 固件项目，没有声称达到车规、功能安全或量产标准。

完成这 35 个工作日后，项目的结束标准不是“屏幕上有数据”，而是代码能构建、硬件能复现、异常能观察、结果有数据、过程能复盘，并且你能在面试中从代码和测试证据出发解释每一个关键设计。
