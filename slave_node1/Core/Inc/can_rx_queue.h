#ifndef CAN_RX_QUEUE_H
#define CAN_RX_QUEUE_H

#include <stdbool.h>
#include <stdint.h>

#include "FreeRTOS.h"
#include "queue.h"
#include "can.h"
#include "usart_driver.h"

typedef struct {
    uint16_t id;
    uint8_t dlc;
    uint8_t data[8];
} CanRxFrame_t;

bool CanRxQueue_Init(void);
void CanRxQueue_PutFromIsr(CAN_HandleTypeDef *hcan);
bool CanRxQueue_Get(CanRxFrame_t *frame, TickType_t wait_ticks);
uint32_t CanRxQueue_GetDropCount(void);

uint32_t CanRxQueue_GetIrqCount(void);

#endif
