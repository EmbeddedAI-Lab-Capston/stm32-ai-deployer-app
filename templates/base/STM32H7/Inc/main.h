#pragma once

#include "stm32h7xx_hal.h"
#include "ai_config.h"

void Error_Handler(void);

#define LED_PIN   GPIO_PIN_3
#define LED_PORT  GPIOB
