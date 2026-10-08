#include "ds18b20.h"
#include "onewire.h"

#define DS18B20_SKIP_ROM  0xCC
#define DS18B20_CONVERT   0x44
#define DS18B20_READ      0xBE

float DS18B20_ReadTemp(void) {
    if (DS18B20_StartConversion()) return -128.0f;
    HAL_Delay(750);
    return DS18B20_ReadTempResult();
}

int DS18B20_StartConversion(void) {
    if (OneWire_Reset()) return -1;
    OneWire_WriteByte(DS18B20_SKIP_ROM);
    OneWire_WriteByte(DS18B20_CONVERT);
    return 0;
}

float DS18B20_ReadTempResult(void) {
    if (OneWire_Reset()) return -128.0f;
    OneWire_WriteByte(DS18B20_SKIP_ROM);
    OneWire_WriteByte(DS18B20_READ);

    uint8_t buf[9];
    for (int i = 0; i < 9; i++) {
        buf[i] = OneWire_ReadByte();
    }

    int16_t raw = (int16_t)(buf[1] << 8) | buf[0];
    return raw / 16.0f;
}
