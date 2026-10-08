
#include "usart_driver.h"

static uint32_t Serial_Pow(uint32_t a, uint32_t b)
{
    uint32_t result = 1;
    while (b--)
        result *= a;
    return result;
}

void Serial_SendByte(uint8_t data)
{
    HAL_UART_Transmit(&huart1, &data, 1, HAL_MAX_DELAY);
}

void Serial_SendArray(const uint8_t *data, uint16_t length)
{
    HAL_UART_Transmit(&huart1, data, length, HAL_MAX_DELAY);
}

void Serial_SendString(const char *str)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)str, strlen(str), HAL_MAX_DELAY);
}

void Serial_SendNum_Raw(uint32_t num, uint8_t length)
{
    uint8_t digits[10];
    for (uint8_t i = 0; i < length; i++)
    {
        digits[i] = (num / Serial_Pow(10, length - i - 1)) % 10;
    }
    HAL_UART_Transmit(&huart1, digits, length, HAL_MAX_DELAY);
}

void Serial_SendNum(uint32_t num, uint8_t length)
{
    char str[12];
    if (length > 0)
        snprintf(str, sizeof(str), "%*d", length, num);
    else
        snprintf(str, sizeof(str), "%d", num);
    Serial_SendString(str);
}

void Serial_Printf(const char *format, ...)
{
    char buf[128];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    Serial_SendString(buf);
}

int fputc(int c, FILE *f)
{
    Serial_SendByte((uint8_t)c);
    return c;
}
