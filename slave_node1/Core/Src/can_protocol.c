#include "can_protocol.h"

/* 例如 value=0x1234，发送数组变为 {0x12, 0x34}。 */
void CanProtocol_PutU16BE(uint8_t *dst, uint16_t value)
{
    dst[0] = (uint8_t)(value >> 8);       /* 高 8 位。 */
    dst[1] = (uint8_t)(value & 0xFFU);    /* 低 8 位。 */
}

uint16_t CanProtocol_GetU16BE(const uint8_t *src)
{
    return (uint16_t)(((uint16_t)src[0] << 8) | src[1]);
}

/* int16_t 的位模式可以先转 uint16_t 再按相同方式发送。 */
void CanProtocol_PutS16BE(uint8_t *dst, int16_t value)
{
    CanProtocol_PutU16BE(dst, (uint16_t)value);
}

int16_t CanProtocol_GetS16BE(const uint8_t *src)
{
    return (int16_t)CanProtocol_GetU16BE(src);
}