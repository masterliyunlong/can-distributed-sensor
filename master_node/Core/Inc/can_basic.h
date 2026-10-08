#ifndef CAN_BASIC_H
#define CAN_BASIC_H

#include <stdbool.h>
#include <stdint.h>
#include <string.h> 
#include "can.h" 
#include "can_protocol.h"
#include "can_rx_queue.h"

/* 初始化过滤器并启动*/
bool CanBasic_Init(void);

/*
 * 发送一个11位标准数据帧
 */
bool CanBasic_Send(uint16_t std_id, const uint8_t *data, uint8_t dlc);

/*
 * 轮询读取fifo一帧
 */
bool CanBasic_Receive(uint16_t *std_id, uint8_t *data, uint8_t *dlc);

//void CanBasic_RxFifo0Callback(CAN_HandleTypeDef *hcan);
//bool CanBasic_TakeReceived(uint16_t *std_id, uint8_t *data, uint8_t *dlc);
uint32_t CanBasic_GetRxIrqCount(void);

#endif
