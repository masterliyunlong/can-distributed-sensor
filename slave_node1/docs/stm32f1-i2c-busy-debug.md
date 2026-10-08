# STM32F103 硬件 I2C BUSY 锁死问题排查与解决

## 背景

- **芯片**: STM32F103C8T6
- **外设**: I2C1 (PB6/PB7), 100kHz, 连接 BH1750 光照传感器 (GY-302 模块)
- **开发环境**: STM32CubeMX 6.18.0 + HAL 库 + Keil MDK-ARM
- **现象**: 串口持续输出 `error`, BH1750 读取失败, 返回 `HAL_BUSY (2)`

---

## 调试过程

### 第一步: 确定错误码

原始代码只打印 `error`, 无法区分具体原因。修改为打印 HAL 返回值:

```c
HAL_StatusTypeDef ret = BH1750_ReadLight(&bh1750, &lux);
if (ret == HAL_OK) {
    printf("Light: %.1f lx\r\n", lux);
} else {
    printf("error, code=%d\r\n", ret);  // 输出具体错误码
}
```

**结果**: `code=2`, 即 `HAL_BUSY`

### 第二步: 排除 BH1750 模块本身

- 换成软件 I2C (GPIO 模拟) 测试: BH1750 正常通信, 读数 425~640 lux — 结论: **模块完好**
- 把 BH1750 从面包板拔掉, BUSY 依然存在 — 结论: **与传感器无关**
- GY-302 自带 4.7k 上拉电阻 — 结论: **上拉电阻不是根因**

### 第三步: 读硬件寄存器

在 `BH1750_Init` 之前直接读 I2C 硬件状态寄存器:

```c
printf("I2C SR2(BUSY)=%08lX\r\n", hi2c1.Instance->SR2);
```

**结果**: `SR2 = 0x00000002`, **bit 1 (BUSY) 置位** — I2C 硬件外设在使能瞬间就锁死了。

在 HAL 库源码中找到返回 `HAL_BUSY` 的位置 (`stm32f1xx_hal_i2c.c:1082`):

```c
// 检查硬件 BUSY 标志, 超时 25ms 未清除就返回 HAL_BUSY
if (I2C_WaitOnFlagUntilTimeout(hi2c, I2C_FLAG_BUSY, SET,
                               I2C_TIMEOUT_BUSY_FLAG, tickstart) != HAL_OK)
{
    return HAL_BUSY;  // <-- 这里
}
```

### 第四步: GPIO 回环测试

用跳线将 PA6↔PB14, PA7↔PB15 连接, 写回环测试: PA6/PA7 输出高低电平, PB14/PB15 读取验证:

```
PASS: SCL=1 SDA=1   // PA6↔PB14 和 PA7↔PB15 接线完全正确
```

结论: PB6/PB7 作为 GPIO 功能正常, 问题出在 **STM32 的硬件 I2C 外设本身**。

---

## 根因分析

`HAL_I2C_Init()` 的执行流程:

```
HAL_I2C_MspInit()       // 配置 PB6/PB7 为 GPIO_MODE_AF_OD (复用开漏)
                         //   此时 ODR 寄存器 = 0  (HAL_GPIO_Init 不设置 ODR)
                         //
__HAL_I2C_DISABLE()      // PE=0, I2C 外设禁用
SWRST 复位
配置 CR2 / CCR / TRISE   // 时序寄存器
__HAL_I2C_ENABLE()       // PE=1  ← 在这里锁死
```

**根因**: `HAL_GPIO_Init` 将 PB6/PB7 配置为 **AF_OD (复用开漏)** 后, ODR 寄存器默认值为 **0**。开漏输出 + ODR=0 → N-MOS 导通 → **PB6/PB7 被拉到 GND**。

随后 `__HAL_I2C_ENABLE` 置 PE=1, I2C 外设启动, 检测到 SDA 为低电平, 误判为"总线上有设备在发 START 信号", BUSY 标志立刻锁死。

**为什么 4.7kΩ 上拉电阻没用?** 上拉电阻对抗的是 MOS 管对地短路, 拉不赢。

**为什么软件 I2C 没问题?** 软件 I2C 使用 `GPIO_MODE_OUTPUT_OD` (纯 GPIO 开漏), 手动 `HAL_GPIO_WritePin(HIGH)` 把 ODR 设为 1 后再操作, 不存在 AF 模式下 ODR 不受控的问题。

**这是 STM32F103 HAL 库的边界 bug** — HAL_I2C_MspInit 完成 GPIO 配置后, 没有先将 SDA/SCL 的 ODR 置 1 就直接使能了 I2C 外设。

---

## 尝试过的软件修复 (均失败)

| 方法 | 原理 | 结果 |
|------|------|------|
| MspInit 后写 `GPIOB->BSRR` 拉高 ODR | MspInit 完成 AF_OD 配置后立即释放引脚 | 失败 — HAL_I2C_Init 内部紧接着 DISABLE/ENABLE 流程, ODR 状态不可控 |
| `__HAL_I2C_ENABLE` 前加 BSRR | 使能 I2C 瞬间确保引脚 HIGH | 失败 |
| 使能前切到 GPIO 模式拉高再切回 AF | 最彻底的引脚复位 | 失败 |
| DeInit + Recovery + ReInit | 总线恢复标准流程 | 失败 |
| 完全绕过 HAL 手动初始化 | 手写 I2C 寄存器, PE 最后置位 | 失败 |

**结论: 硅片层面的行为 — I2C 使能的瞬间硬件内部产生了一个虚假的 START 检测, 软件无法干预。**

---

## 最终方案: 软件 I2C

用两个 GPIO 模拟 I2C 时序, 完全绕过硬件 I2C 外设。

**引脚**: PA6(SCL) + PA7(SDA), 开漏输出, 利用外部上拉电阻 (GY-302 自带 4.7kΩ)。

**架构**:

```
sw_i2c.c/h    — 软件 I2C 驱动 (Start / Stop / SendByte / ReadByte / WaitAck)
bh1750.c/h    — BH1750 协议层 (Init / SetMode / ReadLight)
main.c        — 应用逻辑
```

```c
// sw_i2c.c - 核心: GPIO 位带模拟 I2C 时序
void SW_I2C_SendByte(uint8_t dat) {
    for (int i = 0; i < 8; i++) {
        if (dat & 0x80) SDA_H(); else SDA_L();  // SDA = GPIOA PIN7
        delay_us();
        SCL_H(); delay_us();                     // SCL = GPIOA PIN6
        SCL_L(); delay_us();
        dat <<= 1;
    }
}
```

**结果**: BH1750 正常读数 425~640 lux, 数据稳定。

---

## 技术总结

1. **HAL 库不是银弹** — `HAL_OK` 只表示 API 调用成功, 不代表外设状态正确。遇到问题要直接读硬件寄存器 (SR2, CR1)。
2. **从现象到根因要逐层剥离** — 模块 → 接线 → GPIO → I2C 外设 → HAL 库, 每一步用独立测试验证。
3. **STM32F1 的 I2C 是出了名的坑** — 大量开发者遇到类似 BUSY 锁死问题, 官方 HAL 也未完全解决。
4. **软件 I2C 是可靠的替代方案** — 在 100kHz 标准模式下, GPIO 模拟 I2C 时序完全可以胜任, 且不受硬件外设 bug 影响。代价是多占两个 IO 口和少量 CPU 时间。
5. **遇到硬件 bug 不要死磕** — 确认是硅片问题后, 用软件方案绕过是最务实的做法。
