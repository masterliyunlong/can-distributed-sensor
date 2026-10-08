#ifndef __DS18B20_H__
#define __DS18B20_H__

#include "stm32f1xx_hal.h"

/* Returns temperature in °C, or -128 on error */
float DS18B20_ReadTemp(void);

/* Start conversion, returns 0 on success, -1 on error */
int DS18B20_StartConversion(void);

/* Read result (call after 750ms), returns temp in °C, -128 on error */
float DS18B20_ReadTempResult(void);

#endif
