#include "onewire.h"
#include "delay.h"

#define DQ_PORT GPIOA
#define DQ_PIN  GPIO_PIN_3

/* ── 引脚控制层 ── */
static void DQ_LOW(void)  { HAL_GPIO_WritePin(DQ_PORT, DQ_PIN, GPIO_PIN_RESET); }
static void DQ_HIGH(void) { HAL_GPIO_WritePin(DQ_PORT, DQ_PIN, GPIO_PIN_SET); }
static uint8_t DQ_READ(void) { return HAL_GPIO_ReadPin(DQ_PORT, DQ_PIN); }

/* ── 模式切换：OD输出 / 上拉输入 ── */
static void DQ_Out(void) {
    GPIO_InitTypeDef g = {0};
    g.Pin   = DQ_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(DQ_PORT, &g);
}

static void DQ_In(void) {
    GPIO_InitTypeDef g = {0};
    g.Pin  = DQ_PIN;
    g.Mode = GPIO_MODE_INPUT;
    g.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(DQ_PORT, &g);
}

/* ═══════════════════════════════════
   协议层
   ═══════════════════════════════════ */
void OneWire_Init(void) {
    __HAL_RCC_GPIOA_CLK_ENABLE();
    DQ_Out();
    DQ_HIGH();
}

/* 复位 + 检测存在脉冲 */
uint8_t OneWire_Reset(void) {
    uint8_t presence;

    DQ_Out();
    DQ_LOW();
    Delay_us(500);          /* 拉低 480us 以上 */

    DQ_In();                /* 释放总线, 等待应答 */
    Delay_us(60);           /* 等待 15~60us */

    presence = DQ_READ();   /* 0=有设备, 1=无设备 */
    Delay_us(420);          /* 等待剩余时间, 总共 480us */

    return presence;
}

void OneWire_WriteBit(uint8_t bit) {
    DQ_Out();
    DQ_LOW();
    Delay_us(2);            /* 拉低 >1us */

    if (bit) {
        DQ_In();            /* 写1: 15us 内释放, 上拉维持高 */
        Delay_us(58);
    } else {
        Delay_us(58);       /* 写0: 维持低 60~120us */
        DQ_In();
    }
    Delay_us(2);
}

uint8_t OneWire_ReadBit(void) {
    uint8_t bit;

    DQ_Out();
    DQ_LOW();
    Delay_us(2);            /* 拉低 >1us */

    DQ_In();
    Delay_us(10);           /* 等待数据稳定, 15us 内采样 */
    bit = DQ_READ();
    Delay_us(50);           /* 等待剩余时隙, 总共 >60us */

    return bit;
}

void OneWire_WriteByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        OneWire_WriteBit(byte & 0x01);
        byte >>= 1;         /* 低位在前 */
    }
}

uint8_t OneWire_ReadByte(void) {
    uint8_t byte = 0x00;
    for (uint8_t i = 0; i < 8; i++) {
        if (OneWire_ReadBit()) { byte |= (0x01 << i); }
    }
    return byte;
}
