#ifndef USART_DRIVER_H
#define USART_DRIVER_H

#include "stm32f1xx_hal.h"
#include <stdarg.h>
#include <string.h>
#include <stdio.h>

extern UART_HandleTypeDef huart1;

void Serial_SendByte(uint8_t data);
void Serial_SendArray(const uint8_t *data, uint16_t length);
void Serial_SendString(const char *str);
void Serial_SendNum_Raw(uint32_t num, uint8_t length);
void Serial_SendNum(uint32_t num, uint8_t length);
void Serial_Printf(const char *format, ...);
int fputc(int c, FILE *f);

#endif

