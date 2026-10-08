#ifndef __BH1750_H__
#define __BH1750_H__

#include "stm32f1xx_hal.h"

/* BH1750 commands */
#define BH1750_POWER_ON     0x01
#define BH1750_POWER_OFF    0x00
#define BH1750_CONT_H_MODE  0x10
#define BH1750_CONT_H_MODE2 0x11
#define BH1750_CONT_L_MODE  0x13
#define BH1750_ONE_H_MODE   0x20
#define BH1750_ONE_H_MODE2  0x21
#define BH1750_ONE_L_MODE   0x23

/* Init PA6(SCL)/PA7(SDA) and power on sensor */
void BH1750_Init(void);

/* Send a command byte, returns 0 on success, -1 on NACK */
int BH1750_SendCommand(uint8_t cmd);

/* Set measurement mode */
void BH1750_SetMode(uint8_t mode);

/* Read light intensity in lux, returns 0 on success, -1 on error */
int BH1750_ReadLight(float *lux);

#endif
