#include "can_rx_queue.h"

#include <string.h>

static QueueHandle_t s_rx_queue;
static uint32_t s_rx_drop_count;
static volatile uint32_t s_rx_irq_count;

/*
 * 函数作用：创建接收队列，替代教程版的一个 s_rx_data 数组。
 * 谁调用：启动 FreeRTOS 调度器之前调用一次。
 * 返回值：内存足够并创建成功返回 true；NULL 表示 FreeRTOS 堆不足。
 */
bool CanRxQueue_Init(void)
{
    /* 队列一次保存 16 帧；每帧都是 ID、DLC、8 字节数据。 */
    s_rx_queue = xQueueCreate(16U, sizeof(CanRxFrame_t));
    return s_rx_queue != NULL;
}

/*
 * 函数作用：在 CAN RX0 中断中读取一帧，并把完整帧放入 FreeRTOS 队列。
 * 谁调用：HAL_CAN_RxFifo0MsgPendingCallback()；运行在中断上下文。
 * 参数：hcan 是发生中断的 CAN 句柄。
 * 返回值：无；队列满时不能等待，只累计丢帧数。
 */
void CanRxQueue_PutFromIsr(CAN_HandleTypeDef *hcan)
{	
    CAN_RxHeaderTypeDef header;
    CanRxFrame_t frame;
    /* task_woken 用来告诉 FreeRTOS：是否有高优先级任务应立刻运行。 */
    BaseType_t task_woken = pdFALSE;

    memset(&frame, 0, sizeof(frame));
    if (HAL_CAN_GetRxMessage(hcan, CAN_RX_FIFO0, &header, frame.data) != HAL_OK) {
        return;
    }
    if ((header.IDE != CAN_ID_STD) || (header.RTR != CAN_RTR_DATA) || (header.DLC > 8U)) {
        return;
    }

    frame.id = (uint16_t)header.StdId;
    frame.dlc = header.DLC;
    s_rx_irq_count++; /* 每成功从 FIFO0 读出一帧，记录一次中断接收。 */

    /* 与教程不同的唯一一行：把帧放进队列，而不是覆盖一个全局变量。 */
    if (xQueueSendFromISR(s_rx_queue, &frame, &task_woken) != pdPASS) {
        s_rx_drop_count++;
    }
    portYIELD_FROM_ISR(task_woken);
}

/*
 * 函数作用：普通任务从 RX 队列取一帧。
 * 谁调用：MasterCan_RxTask 或 Slave 的命令接收任务。
 * wait_ticks：portMAX_DELAY 表示没有帧时一直阻塞，不占 CPU。
 * 返回值：取到一帧返回 true；等待超时或参数错误返回 false。
 */
bool CanRxQueue_Get(CanRxFrame_t *frame, TickType_t wait_ticks)
{
    return xQueueReceive(s_rx_queue, frame, wait_ticks) == pdPASS;
}

/* 函数作用：读取“队列满导致丢帧”的次数，用于 Day 13 错误统计。 */
uint32_t CanRxQueue_GetDropCount(void)
{
    return s_rx_drop_count;
}

/* 函数作用：读取 FIFO0 中断成功取帧的次数，用于确认中断真的在工作。 */
uint32_t CanRxQueue_GetIrqCount(void)
{
    return s_rx_irq_count;
}
