#ifndef __SW_I2C_H__
#define __SW_I2C_H__

#include "stm32f1xx_hal.h"

/* Init PA6(SCL)/PA7(SDA) */
void SW_I2C_Init(void);

/* Protocol layer */
void SW_I2C_Start(void);
void SW_I2C_Stop(void);
void SW_I2C_SendByte(uint8_t byte);
uint8_t SW_I2C_ReceiveByte(void);
void SW_I2C_SendAck(uint8_t ack);      /* 0=ACK, 1=NACK */
uint8_t SW_I2C_ReceiveAck(void);       /* returns 0=ACK, 1=NACK */

#endif
