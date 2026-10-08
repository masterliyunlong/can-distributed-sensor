# 跟 DS18B20 对话，记住一句话就够了

## 每次通信都是完全一样的三步

不管你是要读温度、写配置、还是查 ID，流程永远不变：

```
第 1 步：复位 + 握手   （主机喊一声"在吗"，DS18B20 拉低 DQ 回应"在"）
第 2 步：ROM 命令      （告诉 DS18B20 "叫的就是你"）
第 3 步：功能命令      （告诉 DS18B20 "干什么"）
```

跟 I2C 完全不一样。I2C 靠设备地址来区分谁是谁，1-Wire 靠 ROM 命令。

---

## 三步拆开讲

### 第 1 步：复位 + 握手

主机拉低 DQ **至少 480 微秒**，然后松手。如果 DS18B20 在线，它会在 15~60 微秒内自己把 DQ 拉低，持续 60~240 微秒，然后松手。

代码里就一句话：

```c
if (OneWire_Reset()) {
    // 返回 1，说明 DS18B20 没回应 —— 没接好或者坏了
}
// 返回 0，握手成功
```

### 第 2 步：ROM 命令

ROM 命令决定"跟谁说话"。

| 命令 | 值 | 什么时候用 |
|------|-----|-----------|
| SKIP_ROM | 0xCC | 总线上**只有 1 个** DS18B20，跳过寻址 |
| MATCH_ROM | 0x55 | 总线上有多个，用 64 位 ID 指定跟谁说话 |

你的项目就一个 DS18B20，所以永远用 `0xCC`：

```c
OneWire_WriteByte(0xCC);  // "别管 ID 了，就是你"
```

### 第 3 步：功能命令

告诉 DS18B20 "干什么"。

| 命令 | 值 | 干什么 |
|------|-----|--------|
| CONVERT_T | 0x44 | 开始测温度 |
| READ_SCRATCHPAD | 0xBE | 把温度数据读出来 |
| WRITE_SCRATCHPAD | 0x4E | 写配置（改分辨率） |

---

## 完整例子：读一次温度

总共需要**两轮对话**。第一轮发"开始测"，等 750ms；第二轮把结果读出来。

### 第一轮：启动测量

```
OneWire_Reset();              // 第 1 步："在吗"
OneWire_WriteByte(0xCC);      // 第 2 步："就你"
OneWire_WriteByte(0x44);      // 第 3 步："开始测温度"
HAL_Delay(750);               // 等 750ms，12 位精度需要这么久
```

### 第二轮：读结果

```
OneWire_Reset();              // 第 1 步："在吗"
OneWire_WriteByte(0xCC);      // 第 2 步："就你"
OneWire_WriteByte(0xBE);      // 第 3 步："把数据给我"
```

### 数据长什么样

发完 `0xBE` 后，DS18B20 会连续吐出 9 个字节。我们只关心前 2 个：

```
buf[0] = 温度的低 8 位
buf[1] = 温度的高 8 位
```

拼成 16 位有符号数，除以 16，就是 °C：

```c
uint8_t buf[9];
for (int i = 0; i < 9; i++)
    buf[i] = OneWire_ReadByte();

int16_t raw = (buf[1] << 8) | buf[0];
float temp = raw / 16.0f;
```

几个例子帮你理解：

```
25.0°C  → raw = 400  → buf[1]=0x01, buf[0]=0x90 → 400/16 = 25.0
0.5°C   → raw = 8    → buf[1]=0x00, buf[0]=0x08 → 8/16 = 0.5
0.0°C   → raw = 0    → buf[1]=0x00, buf[0]=0x00
-0.5°C  → raw = -8   → buf[1]=0xFF, buf[0]=0xF8 → -8/16 = -0.5
-25.0°C → raw = -400 → buf[1]=0xFE, buf[0]=0x70 → -400/16 = -25.0
```

---

## 代码里对应哪里

### main.c 做的事

```c
OneWire_Init();                       // 配好 PA3 引脚

float temp = DS18B20_ReadTemp();      // 一句话搞定上面所有步骤
printf("Temp: %.1f C\r\n", temp);
```

### ds18b20.c 做的事

把两轮对话封装在 `DS18B20_ReadTemp()` 里：

```c
float DS18B20_ReadTemp(void) {
    // ── 第一轮：启动测量 ──
    if (OneWire_Reset()) return -128.0f;   // 第 1 步
    OneWire_WriteByte(0xCC);               // 第 2 步
    OneWire_WriteByte(0x44);               // 第 3 步
    HAL_Delay(750);

    // ── 第二轮：读数据 ──
    if (OneWire_Reset()) return -128.0f;   // 第 1 步
    OneWire_WriteByte(0xCC);               // 第 2 步
    OneWire_WriteByte(0xBE);               // 第 3 步

    // 收 9 字节，取前 2 个算温度
    uint8_t buf[9];
    for (int i = 0; i < 9; i++)
        buf[i] = OneWire_ReadByte();

    int16_t raw = (buf[1] << 8) | buf[0];
    return raw / 16.0f;
}
```

### onewire.c 做的事

只负责最底层的 bit/btye 收发，它不知道 DS18B20 是什么：

- `OneWire_Reset()` —— 拉低 500μs → 松手 → 检测 DS18B20 有没有把线拉低
- `OneWire_WriteByte()` —— 把一个字节 8 个 bit 依次发出去（低位在前）
- `OneWire_ReadByte()` —— 依次收 8 个 bit，拼成一个字节（低位在前）

---

## 记住这个就行

```
┌────────────┐
│ 第 1 步    │  OneWire_Reset()      —— 喊"在吗"
│ 第 2 步    │  OneWire_WriteByte()  —— 喊"叫谁"  (0xCC = 全叫)
│ 第 3 步    │  OneWire_WriteByte()  —— 喊"干啥"  (0x44 = 测, 0xBE = 读)
└────────────┘
      ↓ 等 750ms (只 0x44 需要)
┌────────────┐
│ 第 1 步    │  OneWire_Reset()
│ 第 2 步    │  OneWire_WriteByte(0xCC)
│ 第 3 步    │  OneWire_WriteByte(0xBE) → 然后 OneWire_ReadByte() × 9
└────────────┘
```

**跟 I2C 最大的区别**：I2C 的地址是硬件接死的（靠 ADDR 引脚），1-Wire 的"地址"是靠命令字选的——0xCC 就是"不管你是谁，听我的就行"。
