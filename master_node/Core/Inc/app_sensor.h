#ifndef APP_SENSOR_H
#define APP_SENSOR_H

#include "FreeRTOS.h"
#include "task.h"
#include "OLED.h"
#include "usart_driver.h"
#include "can_basic.h"
#include "can_rx_queue.h"
#include "can_protocol.h"
#include "button.h"
void sensor_start(void);

#endif
