#include "can_basic.h"




/* 初始化过滤器并启动*/

/* Slave 2 与 Slave 1 相同，只把 0x100 改成 0x101。 */
static bool CanBasic_ConfigSlave2Filter(void)
{
    CAN_FilterTypeDef filter;

    memset(&filter, 0, sizeof(filter));
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDMASK;
    filter.FilterScale = CAN_FILTERSCALE_32BIT;
    filter.FilterIdHigh = (uint16_t)(CAN_ID_S2_COMMAND << 5);
    filter.FilterIdLow = 0U;
    filter.FilterMaskIdHigh = 0xFFE0U;
    filter.FilterMaskIdLow = 0x0006U;
    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    return HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK;
}


bool CanBasic_Init(void){

	if (!CanBasic_ConfigSlave2Filter()) {
			return false;
	}
	if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
		return false;
	}
 
	if (HAL_CAN_Start(&hcan) != HAL_OK) {
			return false;
	}
	return true;
}


/*
 * 发送一个11位标准数据帧
 */
bool CanBasic_Send(uint16_t std_id, const uint8_t *data, uint8_t dlc){
	CAN_TxHeaderTypeDef header;
	uint32_t mailbox;

  
  if ((data == NULL) || (std_id > 0x7FFU) || (dlc > 8U)) {
        return false;
  }
	
	memset(&header, 0, sizeof(header));
	header.StdId=std_id;
	header.ExtId=0;
	header.IDE=CAN_ID_STD;
	header.RTR=CAN_RTR_DATA;
	header.DLC=dlc;
	header.TransmitGlobalTime=DISABLE;
	
	if (HAL_CAN_GetTxMailboxesFreeLevel(&hcan) == 0U) {
        return false;
  }
	
	return HAL_CAN_AddTxMessage(&hcan, &header, (uint8_t *)data, &mailbox) == HAL_OK;
	
}

/*
 * 轮询读取fifo一帧
 */
bool CanBasic_Receive(uint16_t *std_id, uint8_t *data, uint8_t *dlc){
	CAN_RxHeaderTypeDef header;
	if ((std_id == NULL) || (data == NULL) || (dlc == NULL)) {
			return false;
	}
	if (HAL_CAN_GetRxFifoFillLevel(&hcan, CAN_RX_FIFO0) == 0U) {
			return false;
	}
  if (HAL_CAN_GetRxMessage(&hcan, CAN_RX_FIFO0, &header, data) != HAL_OK) {
			return false;
	}
	if ((header.IDE != CAN_ID_STD) || (header.RTR != CAN_RTR_DATA) || (header.DLC > 8U)) {
        return false;
    }
	*std_id = (uint16_t)header.StdId;
  *dlc = header.DLC;
  return true;
}

