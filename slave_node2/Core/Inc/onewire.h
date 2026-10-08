#ifndef __ONEWIRE_H__
#define __ONEWIRE_H__

#include "stm32f1xx_hal.h"

/* Init PA0 as OD output ··· */
void OneWire_Init(void);

/* Returns 0=presence detected, 1=no device */
uint8_t OneWire_Reset(void);

void OneWire_WriteBit(uint8_t bit);
uint8_t OneWire_ReadBit(void);
void OneWire_WriteByte(uint8_t byte);
uint8_t OneWire_ReadByte(void);

#endif
