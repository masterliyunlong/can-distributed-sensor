#include "bh1750.h"
#include "sw_i2c.h"

#define BH1750_ADDR_W 0x46
#define BH1750_ADDR_R 0x47

static uint8_t bh1750_mode = 0;

void BH1750_Init(void) {
    SW_I2C_Init();
    BH1750_SendCommand(BH1750_POWER_ON);
}

int BH1750_SendCommand(uint8_t cmd) {
    SW_I2C_Start();
    SW_I2C_SendByte(BH1750_ADDR_W);
    if (SW_I2C_ReceiveAck()) { SW_I2C_Stop(); return -1; }
    SW_I2C_SendByte(cmd);
    if (SW_I2C_ReceiveAck()) { SW_I2C_Stop(); return -1; }
    SW_I2C_Stop();
    return 0;
}

void BH1750_SetMode(uint8_t mode) {
    bh1750_mode = mode;
    BH1750_SendCommand(mode);
}

int BH1750_ReadLight(float *lux) {
    SW_I2C_Start();
    SW_I2C_SendByte(BH1750_ADDR_R);
    if (SW_I2C_ReceiveAck()) { SW_I2C_Stop(); return -1; }
    uint16_t raw = (uint16_t)SW_I2C_ReceiveByte() << 8;
    SW_I2C_SendAck(0);
    raw |= SW_I2C_ReceiveByte();
    SW_I2C_SendAck(1);
    SW_I2C_Stop();

    float factor = 1.2f;
    if (bh1750_mode == BH1750_CONT_H_MODE2 || bh1750_mode == BH1750_ONE_H_MODE2)
        factor = 1.2f;
    else if (bh1750_mode == BH1750_CONT_L_MODE || bh1750_mode == BH1750_ONE_L_MODE)
        factor = 1.2f / 4.0f;

    *lux = raw / factor;
    return 0;
}
