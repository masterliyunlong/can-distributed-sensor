#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "OLED.h"
#include "gpio.h"
#include "usart_driver.h"
#include "bh1750.h"
#include "adc.h"
#include "semphr.h"
#include "can_basic.h"
#include "can_protocol.h"
#include "can_rx_queue.h"

void sensor_start(void);

#endif
