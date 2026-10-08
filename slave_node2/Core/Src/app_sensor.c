#include "app_sensor.h"

#define START_STACKDEPTH 128
#define START_TASK_PRIORITY 1
TaskHandle_t Start_Task_Handle;
void task_start(void *pvParameters);

#define AdcTask_STACKDEPTH 128
#define AdcTask_PRIORITY 2
TaskHandle_t AdcTask_Handle;
void AdcTask(void *pvParameters);

#define TempTask_STACKDEPTH 128
#define TempTask_PRIORITY 2
TaskHandle_t TempTask_Handle;
void TempTask(void *pvParameters);

#define DataTask_STACKDEPTH 128
#define DataTask_PRIORITY 3
TaskHandle_t DataTask_Handle;
void DataTask(void *pvParameters);


#define UITask_STACKDEPTH 128
#define UITask_PRIORITY 3
TaskHandle_t UITask_Handle;
void UITask(void *pvParameters);


#define Slave2CanTask_STACKDEPTH 128
#define Slave2CanTask_PRIORITY 2
TaskHandle_t Slave2CanTask_Handle;
void Slave2CanTask(void *pvParameters);

#define Slave2CommandTask_STACKDEPTH 192
#define Slave2CommandTask_PRIORITY   3
TaskHandle_t Slave2CommandTask_Handle;
void Slave2CommandTask(void *pvParameters);

typedef struct {
	uint8_t source;   // 0=ADC, 2=DS18B20
	uint8_t valid;    // 1=valid, 0=invalid
	float   value;
} SensorMsg_t;

QueueHandle_t sensorQueue;

/* 0=������1=�Ѿ����ڸ��¸澯״̬�� */
static uint8_t s_temp_alarm_active = 0U;
static uint8_t s_alarm_event_sequence = 0U;

static void Slave2Ui_ShowTemperatureX10(int16_t temperature_x10)
{
    uint16_t magnitude;

    if (temperature_x10 < 0) {
        OLED_ShowChar(3, 6, '-');
        magnitude = (uint16_t)(-(int32_t)temperature_x10);
    } else {
        OLED_ShowChar(3, 6, '+');
        magnitude = (uint16_t)temperature_x10;
    }

    OLED_ShowNum(3, 7, magnitude / 10U, 3U);
    OLED_ShowChar(3, 10, '.');
    OLED_ShowNum(3, 11, magnitude % 10U, 1U);
    OLED_ShowString(3, 12, "C    ");
}


/*����һ�鹲���ڴ�*/
typedef struct {
      uint16_t adc;        // ADC 
      float    light_lux;  // light
      float    temp;       // temp
      uint8_t  adc_valid;
      uint8_t  light_valid;
      uint8_t  temp_valid;
 } SensorSnapshot_t;
SensorSnapshot_t g_snapshot = {0};           // �ڴ����
SemaphoreHandle_t snapshotMutex = NULL;       // �������յĻ����ź���
 

static void Slave2Can_SendTempAlarm(int16_t temperature_x10, uint8_t alarm_active)
{
    uint8_t alarm[8] = {0};

    alarm[0] = 1U;
    alarm[1] = s_alarm_event_sequence++;
    CanProtocol_PutS16BE(&alarm[2], temperature_x10);
    alarm[4] = (alarm_active != 0U) ? 0x01U : 0x00U;
    alarm[5] = 0x02U;

    /* ����ʧ�ܲ��ı� s_temp_alarm_active����һ��״̬�仯��ͨ�� Day 13 ͳ�ơ� */
    (void)CanBasic_Send(CAN_ID_S2_ALARM, alarm, 8U);
}

/* �¶���Чʱ���ã�ֻ�ڡ����롱�򡰽����ʱ����һ���¼�֡�� */
static uint8_t Slave2Can_UpdateTempAlarm(int16_t temperature_x10)
{
    if ((s_temp_alarm_active == 0U) &&
        (temperature_x10 >= S2_TEMP_ALARM_ON_X10)) {
        s_temp_alarm_active = 1U;
        Slave2Can_SendTempAlarm(temperature_x10, 1U);
    } else if ((s_temp_alarm_active != 0U) &&
               (temperature_x10 <= S2_TEMP_ALARM_OFF_X10)) {
        s_temp_alarm_active = 0U;
        Slave2Can_SendTempAlarm(temperature_x10, 0U);
    }

    return s_temp_alarm_active;
}


void sensor_start(void) {
	sensorQueue = xQueueCreate((UBaseType_t) 16,
	                           (UBaseType_t) sizeof(SensorMsg_t));
	// �������յĻ����ź�������
	snapshotMutex= xSemaphoreCreateMutex();
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
	xTaskCreate((TaskFunction_t) AdcTask,
	            (char *) "AdcTask",
	            (configSTACK_DEPTH_TYPE) AdcTask_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) AdcTask_PRIORITY,
	            (TaskHandle_t *) &AdcTask_Handle);

	xTaskCreate((TaskFunction_t) TempTask,
	            (char *) "TempTask",
	            (configSTACK_DEPTH_TYPE) TempTask_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) TempTask_PRIORITY,
	            (TaskHandle_t *) &TempTask_Handle);

	xTaskCreate((TaskFunction_t) DataTask,
	            (char *) "DataTask",
	            (configSTACK_DEPTH_TYPE) DataTask_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) DataTask_PRIORITY,
	            (TaskHandle_t *) &DataTask_Handle);
							
							
  xTaskCreate((TaskFunction_t) UITask,
	            (char *) "UITask",
	            (configSTACK_DEPTH_TYPE) UITask_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) UITask_PRIORITY,
	            (TaskHandle_t *) &UITask_Handle);
							
	xTaskCreate((TaskFunction_t)Slave2CanTask,
				(char *)"Slave2CanTask",
				(configSTACK_DEPTH_TYPE)Slave2CanTask_STACKDEPTH,
				(void *)NULL,
				(UBaseType_t)Slave2CanTask_PRIORITY,
				(TaskHandle_t *)&Slave2CanTask_Handle);
			
	xTaskCreate((TaskFunction_t)Slave2CommandTask,
				(char *)"S2Command",
				(configSTACK_DEPTH_TYPE)Slave2CommandTask_STACKDEPTH,
				(void *)NULL,
				(UBaseType_t)Slave2CommandTask_PRIORITY,
				(TaskHandle_t *)&Slave2CommandTask_Handle);
	taskEXIT_CRITICAL();
	vTaskDelete(NULL);
}


void AdcTask(void *pvParameters) {
	ADC_Read_Satrt();
	TickType_t last_wake = xTaskGetTickCount();
	SensorMsg_t adMsg;

	while (1) {
		uint16_t *ad = Get_AD_Val();
		adMsg.source = 0;
		adMsg.valid  = 1;
		adMsg.value  = (float)ad[0];
		xQueueSend(sensorQueue, &adMsg, portMAX_DELAY);

		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(100));
	}
}

void TempTask(void *pvParameters) {
	OneWire_Init();
	float temp = 0.0f;
	TickType_t last_wake = xTaskGetTickCount();
	SensorMsg_t tpMsg;

	while (1) {
		tpMsg.source = 2;
		taskENTER_CRITICAL();
		if (DS18B20_StartConversion() == 0) {
			taskEXIT_CRITICAL();
			vTaskDelay(pdMS_TO_TICKS(750));
			taskENTER_CRITICAL();
			temp = DS18B20_ReadTempResult();
			taskEXIT_CRITICAL();
			if (temp != -128.0f) {
				tpMsg.valid = 1;
				tpMsg.value = temp;
			} else {
				tpMsg.valid = 0;
				tpMsg.value = 0.0f;
			}
		} else {
			taskEXIT_CRITICAL();
			tpMsg.valid = 0;
			tpMsg.value = 0.0f;
		}
		xQueueSend(sensorQueue, &tpMsg, portMAX_DELAY);

		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(1000));
	}
}

void DataTask(void *pvParameters) {
	SensorMsg_t msg;

	while (1) {
		xQueueReceive(sensorQueue, &msg, portMAX_DELAY);
		xSemaphoreTake( snapshotMutex,
                 portMAX_DELAY);

		switch (msg.source) {
		case 0:
			g_snapshot.adc = (uint16_t)msg.value;
      g_snapshot.adc_valid = msg.valid;
			break;
//		case 1:
//			g_snapshot.light_lux = (float)msg.value;
//      g_snapshot.light_valid = msg.valid;
//			break;
		case 2:
			g_snapshot.temp = (float)msg.value;
      g_snapshot.temp_valid = msg.valid;
			break;
		}
		xSemaphoreGive( snapshotMutex );

	}
}

void UITask(void *pvParameters) {
	
	SensorSnapshot_t local;
	TickType_t last_wake = xTaskGetTickCount();
	int16_t displayed_temp_x10 = 0;
	uint8_t displayed_temp_valid = 0U;
	uint8_t temp_drawn = 0U;
	
	while (1) {
		xSemaphoreTake( snapshotMutex,
                 portMAX_DELAY);
		local=g_snapshot;
		xSemaphoreGive( snapshotMutex );
		OLED_ShowString(1, 1, "AD:");
		if (local.adc_valid)
				OLED_ShowNum(1, 5, local.adc, 4);

//		OLED_ShowString(2, 1, "Light:");
//		if (local.light_valid)
//				OLED_ShowNum(2, 7, (uint32_t)local.light_lux, 4);

		{
			int16_t temp_x10 = (int16_t)(local.temp * 10.0f);
			if ((temp_drawn == 0U) ||
				(displayed_temp_valid != local.temp_valid) ||
				((local.temp_valid != 0U) &&
				 (displayed_temp_x10 != temp_x10))) {
				OLED_ShowString(3, 1, "Temp:           ");
				if (local.temp_valid != 0U) {
					Slave2Ui_ShowTemperatureX10(temp_x10);
				}
				displayed_temp_x10 = temp_x10;
				displayed_temp_valid = local.temp_valid;
				temp_drawn = 1U;
			}
		}

		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));
	

	}
}


void Slave2CanTask(void *pvParameters)
{
    uint8_t data[8] = {0};
    uint8_t sequence = 0U;
    uint32_t query_sequence = 0U;
    BaseType_t is_query_reply;
    SensorSnapshot_t local;
    int16_t temperature_x10;

    (void)pvParameters;

    while (1) {
        is_query_reply = xTaskNotifyWait(0U,
                                         UINT32_MAX,
                                         &query_sequence,
                                         pdMS_TO_TICKS(2000U));

        xSemaphoreTake(snapshotMutex, portMAX_DELAY);
        local = g_snapshot;
        xSemaphoreGive(snapshotMutex);

        temperature_x10 = (int16_t)(local.temp * 10.0f);

        data[0] = 1U;
        data[1] = sequence++;
        CanProtocol_PutU16BE(&data[2], local.adc);
        CanProtocol_PutS16BE(&data[4], temperature_x10);

        data[6] = 0U;
        if (local.adc_valid != 0U) {
            data[6] |= 0x01U;       /* bit0��ADC ��Ч */
        }
        if (local.temp_valid != 0U) {
            data[6] |= 0x04U;       /* bit2���¶���Ч */

            if (Slave2Can_UpdateTempAlarm(temperature_x10) != 0U) {
                data[6] |= 0x08U;   /* bit3����ǰ�����ڸ��� */
            }
        } else {
            /* �¶ȴ�����ʧЧ���ܱ����ɸ澯�������͡�������¼�����Ϊ�¶�δ֪�� */
            s_temp_alarm_active = 0U;
        }

        if (is_query_reply == pdTRUE) {
            data[6] |= 0x80U;       /* bit7����ѯ�ظ� */
            data[7] = (uint8_t)query_sequence;
        } else {
            data[7] = 0U;
        }

        (void)CanBasic_Send(CAN_ID_S2_TELEMETRY, data, 8U);
    }
}

void Slave2CommandTask(void *pvParameters)
{
    CanRxFrame_t frame;

    (void)pvParameters;

    while (1) {
        if (!CanRxQueue_Get(&frame, portMAX_DELAY)) {
            continue;
        }

        if ((frame.id == CAN_ID_S2_COMMAND) &&
            (frame.dlc == 8U) &&
            (frame.data[0] == 1U) &&
            (frame.data[2] == CAN_COMMAND_QUERY_NOW)) {
            xTaskNotify(Slave2CanTask_Handle,
                        (uint32_t)frame.data[1],
                        eSetValueWithOverwrite);
        }
    }
}


