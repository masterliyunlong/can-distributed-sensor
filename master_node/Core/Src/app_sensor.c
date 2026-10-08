#include "app_sensor.h"

#define START_STACKDEPTH 128
#define START_TASK_PRIORITY 1
TaskHandle_t Start_Task_Handle;
void task_start(void *pvParameters);

#define CanTask_STACKDEPTH 256
#define CanTask_PRIORITY 3
TaskHandle_t CanTask_Handle;
void CanTask(void *pvParameters);

#define ButtonTask_STACKDEPTH 128
#define ButtonTask_PRIORITY 2
TaskHandle_t ButtonTask_Handle;
void ButtonTask(void *pvParameters);

// �������յĻ����ź���

typedef struct {
    uint16_t s1_adc;
    uint16_t s1_light;
    uint16_t s2_adc;
    int16_t  s2_temp_x10;
    uint8_t  s1_status;
    uint8_t  s2_status;
    uint8_t  s2_temp_alarm;
    TickType_t s1_last_tick;
    TickType_t s2_last_tick;
    uint8_t s1_online;
    uint8_t s2_online;
} MasterCanData_t;
 

typedef struct {
    uint8_t pending;       /* 1=���ڵȴ��ظ���0=��ǰû�в�ѯ�� */
    uint8_t node;          /* 1=Slave 1��2=Slave 2�� */
    uint8_t sequence;      /* �������� Byte1�� */
    TickType_t sent_tick;  /* �����ʱ�̡� */
} QueryWait_t;

static QueryWait_t s_query_wait;
 

void sensor_start(void) {
	xTaskCreate((TaskFunction_t) task_start,
	            (char *) "task_start",
	            (configSTACK_DEPTH_TYPE) START_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) START_TASK_PRIORITY,
	            (TaskHandle_t *) &Start_Task_Handle);

	vTaskStartScheduler();
}

void task_start(void *pvParameters) {
    taskENTER_CRITICAL();

    xTaskCreate((TaskFunction_t)CanTask,
                (char *)"CanTask",
                (configSTACK_DEPTH_TYPE)CanTask_STACKDEPTH,
                (void *)NULL,
                (UBaseType_t)CanTask_PRIORITY,
                (TaskHandle_t *)&CanTask_Handle);

    xTaskCreate((TaskFunction_t)ButtonTask,
                (char *)"ButtonTask",
                (configSTACK_DEPTH_TYPE)ButtonTask_STACKDEPTH,
                (void *)NULL,
                (UBaseType_t)ButtonTask_PRIORITY,
                (TaskHandle_t *)&ButtonTask_Handle);

    taskEXIT_CRITICAL();
    vTaskDelete(NULL);
}
static void MasterCan_ShowTemperatureX10(uint8_t line, uint8_t column,
                                         int16_t temperature_x10)
{
    uint16_t magnitude;

    if (temperature_x10 < 0) {
        OLED_ShowChar(line, column, '-');
        magnitude = (uint16_t)(-(int32_t)temperature_x10);
    } else {
        OLED_ShowChar(line, column, '+');
        magnitude = (uint16_t)temperature_x10;
    }

    OLED_ShowNum(line, column + 1U, magnitude / 10U, 3U);
    OLED_ShowChar(line, column + 4U, '.');
    OLED_ShowNum(line, column + 5U, magnitude % 10U, 1U);
}

static void MasterCan_DrawS1(const MasterCanData_t *data)
{
    if (data->s1_online != 0U) {
        OLED_ShowString(1, 1, "S1AD:          ");
        OLED_ShowNum(1, 7, data->s1_adc, 4);
        OLED_ShowChar(1, 16,
                      ((data->s1_status & 0x80U) != 0U) ? 'Q' : ' ');
        OLED_ShowString(2, 1, "LIGHT:          ");
        OLED_ShowNum(2, 7, data->s1_light, 5);
    } else {
        OLED_ShowString(1, 1, "S1 OFFLINE      ");
        OLED_ShowString(2, 1, "                ");
    }
}

static void MasterCan_DrawS2(const MasterCanData_t *data)
{
    if (data->s2_online != 0U) {
        OLED_ShowString(3, 1, "S2AD:          ");
        OLED_ShowNum(3, 7, data->s2_adc, 4);
        OLED_ShowChar(3, 16,
                      ((data->s2_status & 0x80U) != 0U) ? 'Q' : ' ');
        OLED_ShowString(4, 1, "T:              ");
        MasterCan_ShowTemperatureX10(4, 3, data->s2_temp_x10);
        if (data->s2_temp_alarm != 0U) {
            OLED_ShowString(4, 12, "HIGH");
        }
    } else {
        OLED_ShowString(3, 1, "S2 OFFLINE      ");
        OLED_ShowString(4, 1, "                ");
    }
}

void CanTask(void *pvParameters)
{
    CanRxFrame_t frame;
    MasterCanData_t latest = {0};
    MasterCanData_t displayed = {0};
    uint8_t s1_drawn = 0U;
    uint8_t s2_drawn = 0U;
		
		/* �ɹ���ӡ�ظ� */
		uint8_t reply_node;
		uint8_t reply_matched;
		uint32_t latency_ms;
		
		/* ����500msδ�ظ� */
		uint8_t timeout_node;
		uint8_t timeout_sequence;
		uint8_t timed_out;
		uint8_t frame_received;

    (void)pvParameters;

    while (1) {
        frame_received = CanRxQueue_Get(&frame, pdMS_TO_TICKS(200U)) ? 1U : 0U;
        if (frame_received != 0U) {
            if ((frame.id == CAN_ID_S1_TELEMETRY) &&
                (frame.dlc == 8U) && (frame.data[0] == 1U)) {
                latest.s1_adc = CanProtocol_GetU16BE(&frame.data[2]);
                latest.s1_light = CanProtocol_GetU16BE(&frame.data[4]);
                latest.s1_status = frame.data[6];
                latest.s1_last_tick = xTaskGetTickCount();
                latest.s1_online = 1U;
            } else if ((frame.id == CAN_ID_S2_TELEMETRY) &&
                       (frame.dlc == 8U) && (frame.data[0] == 1U)) {
                latest.s2_adc = CanProtocol_GetU16BE(&frame.data[2]);
                latest.s2_temp_x10 = CanProtocol_GetS16BE(&frame.data[4]);
                latest.s2_status = frame.data[6];
                latest.s2_temp_alarm =
                    ((frame.data[6] & 0x08U) != 0U) ? 1U : 0U;
                latest.s2_last_tick = xTaskGetTickCount();
                latest.s2_online = 1U;
            } else if ((frame.id == CAN_ID_S2_ALARM) &&
                       (frame.dlc == 8U) && (frame.data[0] == 1U) &&
                       (frame.data[5] == 0x02U)) {
                latest.s2_temp_x10 = CanProtocol_GetS16BE(&frame.data[2]);
                latest.s2_temp_alarm = (frame.data[4] == 0x01U) ? 1U : 0U;
            }
        }
				
				if ((frame_received != 0U) &&
						(frame.dlc == 8U) &&
						(frame.data[0] == 1U) &&
						((frame.id == CAN_ID_S1_TELEMETRY) ||
						 (frame.id == CAN_ID_S2_TELEMETRY)) &&
						((frame.data[6] & 0x80U) != 0U)) 
				{

									reply_node = (frame.id == CAN_ID_S1_TELEMETRY) ? 1U : 2U;
									reply_matched = 0U;
									latency_ms = 0U;

									taskENTER_CRITICAL();
									if ((s_query_wait.pending != 0U) &&
											(s_query_wait.node == reply_node) &&
											(s_query_wait.sequence == frame.data[7])) 
									{
											latency_ms = (uint32_t)(xTaskGetTickCount() -
																							s_query_wait.sent_tick);
											s_query_wait.pending = 0U;
											reply_matched = 1U;
									}
									taskEXIT_CRITICAL();

									if (reply_matched != 0U) {
											Serial_Printf("S%u reply seq=%u latency=%lu ms\r\n",
																		reply_node,
																		frame.data[7],
																		(unsigned long)latency_ms);
										}
				}
					
				
        if ((latest.s1_online != 0U) &&
            ((xTaskGetTickCount() - latest.s1_last_tick) > pdMS_TO_TICKS(5000U))) {
            latest.s1_online = 0U;
        }
        if ((latest.s2_online != 0U) &&
            ((xTaskGetTickCount() - latest.s2_last_tick) > pdMS_TO_TICKS(5000U))) {
            latest.s2_online = 0U;
            latest.s2_temp_alarm = 0U;
        }

        if ((s1_drawn == 0U) ||
            (displayed.s1_online != latest.s1_online) ||
            (displayed.s1_adc != latest.s1_adc) ||
            (displayed.s1_light != latest.s1_light) ||
            (displayed.s1_status != latest.s1_status)) 
				{
            MasterCan_DrawS1(&latest);
            displayed.s1_online = latest.s1_online;
            displayed.s1_adc = latest.s1_adc;
            displayed.s1_light = latest.s1_light;
            displayed.s1_status = latest.s1_status;
            s1_drawn = 1U;
        }

        if ((s2_drawn == 0U) ||
            (displayed.s2_online != latest.s2_online) ||
            (displayed.s2_adc != latest.s2_adc) ||
            (displayed.s2_temp_x10 != latest.s2_temp_x10) ||
            (displayed.s2_status != latest.s2_status) ||
            (displayed.s2_temp_alarm != latest.s2_temp_alarm)) 
				{
            MasterCan_DrawS2(&latest);
            displayed.s2_online = latest.s2_online;
            displayed.s2_adc = latest.s2_adc;
            displayed.s2_temp_x10 = latest.s2_temp_x10;
            displayed.s2_status = latest.s2_status;
            displayed.s2_temp_alarm = latest.s2_temp_alarm;
            s2_drawn = 1U;
        }
						
				
				timed_out = 0U;
				timeout_node = 0U;
				timeout_sequence = 0U;

				taskENTER_CRITICAL();
				if ((s_query_wait.pending != 0U) &&
						((xTaskGetTickCount() - s_query_wait.sent_tick) >
						 pdMS_TO_TICKS(500U))) 
				{
						timeout_node = s_query_wait.node;
						timeout_sequence = s_query_wait.sequence;
						s_query_wait.pending = 0U;
						timed_out = 1U;
				}
				taskEXIT_CRITICAL();

				if (timed_out != 0U) {
						Serial_Printf("S%u timeout seq=%u\r\n",
													timeout_node,
													timeout_sequence);
				}
    }
}

void ButtonTask(void *pvParameters)
{
    uint8_t command[8] = {1U, 0U, CAN_COMMAND_QUERY_NOW, 0U, 0U, 0U, 0U, 0U};
    uint8_t command_sequence_s1 = 0U;
    uint8_t command_sequence_s2 = 0U;
    uint8_t key;
		
		
    (void)pvParameters;

    while (1) {
			uint16_t command_id;
			uint8_t target_node;
			uint8_t can_send_ok;

       key = KEY_INPUT();

			/* ��һ�β�ѯ����ǰ�����ܵڶ��β�ѯ��������ű����ǡ� */
			if ((key != 0U) && (s_query_wait.pending == 0U)) {
					if (key == 1U) {
							target_node = 1U;
							command_id = CAN_ID_S1_COMMAND;
							command[1] = command_sequence_s1++;
					} else {
							target_node = 2U;
							command_id = CAN_ID_S2_COMMAND;
							command[1] = command_sequence_s2++;
					}

					/* �ȵǼǵȴ�״̬����ֹ�ܿ쵽��Ļظ����ڵǼ�ǰ�� CanTask ������ */
					taskENTER_CRITICAL();
					s_query_wait.pending = 1U;
					s_query_wait.node = target_node;
					s_query_wait.sequence = command[1];
					s_query_wait.sent_tick = xTaskGetTickCount();
					taskEXIT_CRITICAL();

					can_send_ok = CanBasic_Send(command_id, command, 8U) ? 1U : 0U;
					if (can_send_ok == 0U) {
							taskENTER_CRITICAL();
							s_query_wait.pending = 0U;
							taskEXIT_CRITICAL();
							Serial_Printf("S%u query mailbox failed\r\n", target_node);
					}
			}

			vTaskDelay(pdMS_TO_TICKS(10U));
    }
}