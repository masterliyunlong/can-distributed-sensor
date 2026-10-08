#include "app_sensor.h"

#define START_STACKDEPTH 128
#define START_TASK_PRIORITY 1
TaskHandle_t Start_Task_Handle;
void task_start(void *pvParameters);

#define AdcTask_STACKDEPTH 128
#define AdcTask_PRIORITY 2
TaskHandle_t AdcTask_Handle;
void AdcTask(void *pvParameters);

#define LightTask_STACKDEPTH 128
#define LightTask_PRIORITY 2
TaskHandle_t LightTask_Handle;
void LightTask(void *pvParameters);


#define DataTask_STACKDEPTH 128
#define DataTask_PRIORITY 3
TaskHandle_t DataTask_Handle;
void DataTask(void *pvParameters);


#define UITask_STACKDEPTH 128
#define UITask_PRIORITY 3
TaskHandle_t UITask_Handle;
void UITask(void *pvParameters);

#define Slave1CanTask_STACKDEPTH 128
#define Slave1CanTask_PRIORITY 2
TaskHandle_t Slave1CanTask_Handle;
void Slave1CanTask(void *pvParameters);

#define Slave1CommandTask_STACKDEPTH 192
#define Slave1CommandTask_PRIORITY   3
TaskHandle_t Slave1CommandTask_Handle;
void Slave1CommandTask(void *pvParameters);

typedef struct {
	uint8_t source;   // 0=ADC, 1=BH1750
	uint8_t valid;    // 1=valid, 0=invalid
	float   value;
} SensorMsg_t;

QueueHandle_t sensorQueue;


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

	xTaskCreate((TaskFunction_t) LightTask,
	            (char *) "LightTask",
	            (configSTACK_DEPTH_TYPE) LightTask_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) LightTask_PRIORITY,
	            (TaskHandle_t *) &LightTask_Handle);


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
							
	xTaskCreate((TaskFunction_t) Slave1CanTask,
	            (char *) "Slave1CanTask",
	            (configSTACK_DEPTH_TYPE) Slave1CanTask_STACKDEPTH,
	            (void *) NULL,
	            (UBaseType_t) Slave1CanTask_PRIORITY,
	            (TaskHandle_t *) &Slave1CanTask_Handle);
				
	xTaskCreate((TaskFunction_t)Slave1CommandTask,
				(char *)"S1Command",
				(configSTACK_DEPTH_TYPE)Slave1CommandTask_STACKDEPTH,
				(void *)NULL,
				(UBaseType_t)Slave1CommandTask_PRIORITY,
				(TaskHandle_t *)&Slave1CommandTask_Handle);
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

void LightTask(void *pvParameters) {
	BH1750_Init();
	float lux = 0.0f;
	TickType_t last_wake = xTaskGetTickCount();
	SensorMsg_t ltMsg;

	while (1) {
		taskENTER_CRITICAL();
		BH1750_SendCommand(BH1750_ONE_H_MODE);
		taskEXIT_CRITICAL();
		vTaskDelay(pdMS_TO_TICKS(180));

		ltMsg.source = 1;
		taskENTER_CRITICAL();
		if (BH1750_ReadLight(&lux) == 0) {
			taskEXIT_CRITICAL();
			ltMsg.valid = 1;
			ltMsg.value = lux;
		} else {
			taskEXIT_CRITICAL();
			ltMsg.valid = 0;
			ltMsg.value = 0.0f;
		}
		xQueueSend(sensorQueue, &ltMsg, portMAX_DELAY);

		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(500));
	}
}

void DataTask(void *pvParameters) {
	SensorMsg_t msg;

	while (1) {
		xQueueReceive(sensorQueue, &msg, portMAX_DELAY);
		xSemaphoreTake(snapshotMutex,
                 portMAX_DELAY);

		switch (msg.source) {
		case 0:
			g_snapshot.adc = (uint16_t)msg.value;
      g_snapshot.adc_valid = msg.valid;
			break;
		case 1:
			g_snapshot.light_lux = (float)msg.value;
      g_snapshot.light_valid = msg.valid;
			break;
//		case 2:
//			g_snapshot.temp = (float)msg.value;
//      g_snapshot.temp_valid = msg.valid;
//			break;
		}
		xSemaphoreGive( snapshotMutex );

	}
}

void UITask(void *pvParameters) {
	
	SensorSnapshot_t local;
	TickType_t last_wake = xTaskGetTickCount();
	
	while (1) {
		xSemaphoreTake( snapshotMutex,
                 portMAX_DELAY);
		local=g_snapshot;
		xSemaphoreGive( snapshotMutex );
		OLED_ShowString(1, 1, "AD:");
		if (local.adc_valid)
				OLED_ShowNum(1, 5, local.adc, 4);

		OLED_ShowString(2, 1, "Light:");
		if (local.light_valid)
				OLED_ShowNum(2, 7, (uint32_t)local.light_lux, 4);

//		OLED_ShowString(3, 1, "Temp:");
//		if (local.temp_valid)
//				OLED_ShowNum(3, 6, (uint32_t)local.temp, 4);

		vTaskDelayUntil(&last_wake, pdMS_TO_TICKS(200));
	

	}
}

void Slave1CanTask(void *pvParameters) {
		
	
	  SensorSnapshot_t local;
    uint8_t data[8] = {0};
    uint8_t sequence = 0U;
		
		uint32_t query_sequence;
		BaseType_t is_query_reply;

    (void)pvParameters;

    while (1) {
			
				
				is_query_reply = xTaskNotifyWait(0U,
                                    UINT32_MAX,
                                    &query_sequence,
                                    pdMS_TO_TICKS(2000U));

 
        xSemaphoreTake(snapshotMutex, portMAX_DELAY);
        local = g_snapshot;
        xSemaphoreGive(snapshotMutex);

        data[0] = 1U;
        data[1] = sequence++;
        CanProtocol_PutU16BE(&data[2], local.adc);
        CanProtocol_PutU16BE(&data[4], (uint16_t)local.light_lux);

        data[6] = 0U;
        if (local.adc_valid != 0U) {
            data[6] |= 0x01U;
        }
        if (local.light_valid != 0U) {
            data[6] |= 0x02U;
        }
				
        if (is_query_reply == pdTRUE) {
						data[6] |= 0x80U;
						data[7] = (uint8_t)query_sequence;
				} else {
						data[7] = 0U;
				}
				
        (void)CanBasic_Send(CAN_ID_S1_TELEMETRY, data, 8U);
    }
}



void Slave1CommandTask(void *pvParameters)
{
    CanRxFrame_t frame;

    (void)pvParameters;

    while (1) {
        if (!CanRxQueue_Get(&frame, portMAX_DELAY)) {
            continue;
        }

        if ((frame.id == CAN_ID_S1_COMMAND) &&
            (frame.dlc == 8U) &&
            (frame.data[0] == 1U) &&
            (frame.data[2] == CAN_COMMAND_QUERY_NOW)) {
            xTaskNotify(Slave1CanTask_Handle,
                        (uint32_t)frame.data[1],
                        eSetValueWithOverwrite);
        }
    }
}




