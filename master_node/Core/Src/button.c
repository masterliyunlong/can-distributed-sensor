
#include "button.h"

uint8_t KEY_INPUT(void){
	uint8_t KEY_NUM=0;
	if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_1)==GPIO_PIN_RESET){
		HAL_Delay(20);
		if (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1) == GPIO_PIN_RESET) {
			while (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_1) == GPIO_PIN_RESET);
			return 1;
		}
	}
	if(HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11)==GPIO_PIN_RESET){
		HAL_Delay(20);
		if (HAL_GPIO_ReadPin(GPIOB,GPIO_PIN_11) == GPIO_PIN_RESET) {
			while (HAL_GPIO_ReadPin(GPIOB, GPIO_PIN_11) == GPIO_PIN_RESET);
			return 2;
		}
	}
	return KEY_NUM;
}

