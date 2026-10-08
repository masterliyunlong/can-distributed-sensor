#include "can_basic.h"


static volatile uint32_t s_rx_irq_count;

uint32_t CanBasic_GetRxIrqCount(void)
{
    return s_rx_irq_count;
}

/* 初始化CAN和过滤器 */

/* Master 使用 16 位列表过滤器：0x180、0x181、0x081 标准帧 */
static bool CanBasic_ConfigMasterFilter(void)
{
    CAN_FilterTypeDef filter;

    memset(&filter, 0, sizeof(filter));
    filter.FilterBank = 0;
    filter.FilterMode = CAN_FILTERMODE_IDLIST;     /* 列表模式，不依赖掩码模式 */
    filter.FilterScale = CAN_FILTERSCALE_16BIT;    /* 一个 bank 存四个列表位置 */

    /* 标准 ID 在寄存器中左对齐到高 11 位 */
    filter.FilterIdHigh = (uint16_t)(CAN_ID_S1_TELEMETRY << 5);
    filter.FilterIdLow = (uint16_t)(CAN_ID_S2_TELEMETRY << 5);
    filter.FilterMaskIdHigh = (uint16_t)(CAN_ID_S2_ALARM << 5);
    filter.FilterMaskIdLow = 0xFFFFU;              /* 第四个位置无效，仅匹配扩展远程帧 */

    filter.FilterFIFOAssignment = CAN_FILTER_FIFO0;
    filter.FilterActivation = ENABLE;
    filter.SlaveStartFilterBank = 14;
    return HAL_CAN_ConfigFilter(&hcan, &filter) == HAL_OK;
}





bool CanBasic_Init(void){

	if (!CanBasic_ConfigMasterFilter()) {
		return false;
	}

	if (HAL_CAN_Start(&hcan) != HAL_OK) {
		return false;
	}

	/*
	     * 关联 HAL 在 FIFO0 收到消息时调用 RxFifo0MsgPendingCallback。
	     * 没有关联时，即便 IRQ 已使能，也不会调用你写的接收回调
	     */
	if (HAL_CAN_ActivateNotification(&hcan, CAN_IT_RX_FIFO0_MSG_PENDING) != HAL_OK) {
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




////* 中断和循环均使用，所以标记 volatile */
//static volatile uint8_t s_rx_flag;
//static uint16_t s_rx_id;
//static uint8_t s_rx_dlc;
//static uint8_t s_rx_data[8];

///* 由用户回调，被 HAL 在 USB_LP_CAN1_RX0_IRQn 中调用 */
//void CanBasic_RxFifo0Callback(CAN_HandleTypeDef *can_handle)
//{
//    CAN_RxHeaderTypeDef header;

//   /* 只处理默认的 CAN1 */
//    if (can_handle != &hcan) {
//        return;
//    }

//    /* 从 FIFO0 取一帧 */
//    if (HAL_CAN_GetRxMessage(can_handle, CAN_RX_FIFO0, &header, s_rx_data) != HAL_OK) {
//       return;
//    }

//    if ((header.IDE != CAN_ID_STD) || (header.RTR != CAN_RTR_DATA) || (header.DLC > 8U)) {
//        return;
//    }
//	
//    s_rx_id = (uint16_t)header.StdId;
//    s_rx_dlc = header.DLC;
//    s_rx_flag = 1U; /* 主循环轮询 1 代表有新数据 */
//	s_rx_irq_count++;
//}

//bool CanBasic_TakeReceived(uint16_t *std_id, uint8_t *data, uint8_t *dlc)
//{
//    if ((std_id == NULL) || (data == NULL) || (dlc == NULL) || (s_rx_flag == 0U)) {
//        return false;
//    }

//    /* 清除标志再拷贝数据，此版本一次只能保留一帧 */
//    s_rx_flag = 0U;
//    *std_id = s_rx_id;
//    *dlc = s_rx_dlc;
//    memcpy(data, s_rx_data, s_rx_dlc);
//    return true;
//}
