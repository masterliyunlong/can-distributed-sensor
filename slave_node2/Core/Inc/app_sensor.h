#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "OLED.h"
#include "gpio.h"
#include "usart_driver.h"
#include "ds18b20.h"
#include "onewire.h"
#include "adc.h"
#include "semphr.h"

#include "can_basic.h"
#include "can_protocol.h"
#include "can_rx_queue.h"

#define S2_TEMP_ALARM_ON_X10   300     /* 30.0�� */
#define S2_TEMP_ALARM_OFF_X10  280     /* 28.0�� */



void sensor_start(void);

#endif
