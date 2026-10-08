#include "sw_i2c.h"
#include "delay.h"

#define SCL_PORT GPIOA
#define SCL_PIN  GPIO_PIN_6
#define SDA_PORT GPIOA
#define SDA_PIN  GPIO_PIN_7

/* ═══════════════════════════════════
   引脚控制层
   ═══════════════════════════════════ */
static void W_SCL(uint8_t val) {
    HAL_GPIO_WritePin(SCL_PORT, SCL_PIN, val ? GPIO_PIN_SET : GPIO_PIN_RESET);
    Delay_us(10);
}

static void W_SDA(uint8_t val) {
    HAL_GPIO_WritePin(SDA_PORT, SDA_PIN, val ? GPIO_PIN_SET : GPIO_PIN_RESET);
    Delay_us(10);
}

static uint8_t R_SDA(void) {
    uint8_t val = HAL_GPIO_ReadPin(SDA_PORT, SDA_PIN);
    Delay_us(10);
    return val;
}

/* ═══════════════════════════════════
   协议层
   ═══════════════════════════════════ */
void SW_I2C_Init(void) {
    /* PA6=SCL, PA7=SDA, 开漏输出 */
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin   = SCL_PIN | SDA_PIN;
    g.Mode  = GPIO_MODE_OUTPUT_OD;
    g.Speed = GPIO_SPEED_FREQ_HIGH;
    HAL_GPIO_Init(GPIOA, &g);

    W_SCL(1);
    W_SDA(1);
}

void SW_I2C_Start(void) {
    W_SDA(1);
    W_SCL(1);
    W_SDA(0);
    W_SCL(0);
}

void SW_I2C_Stop(void) {
    W_SDA(0);
    W_SCL(1);
    W_SDA(1);
}

void SW_I2C_SendByte(uint8_t byte) {
    for (uint8_t i = 0; i < 8; i++) {
        W_SDA(!!(byte & (0x80 >> i)));
        W_SCL(1);
        W_SCL(0);
    }
}

uint8_t SW_I2C_ReceiveByte(void) {
    uint8_t byte = 0x00;
    W_SDA(1);
    for (uint8_t i = 0; i < 8; i++) {
        W_SCL(1);
        if (R_SDA()) { byte |= (0x80 >> i); }
        W_SCL(0);
    }
    return byte;
}

void SW_I2C_SendAck(uint8_t ack) {
    W_SDA(ack);
    W_SCL(1);
    W_SCL(0);
}

uint8_t SW_I2C_ReceiveAck(void) {
    uint8_t ack;
    W_SDA(1);
    W_SCL(1);
    ack = R_SDA();
    W_SCL(0);
    return ack;
}
