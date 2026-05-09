#pragma once

#include "stm32n6xx_hal.h"
#include "ai_config.h"

void Error_Handler(void);

#define LED_PIN   GPIO_PIN_6
#define LED_PORT  GPIOE
