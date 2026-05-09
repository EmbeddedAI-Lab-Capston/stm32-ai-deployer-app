#pragma once

#include "stm32f4xx_hal.h"
#include "ai_config.h"

/* Exported function prototypes */
void Error_Handler(void);

/* User-defined pin aliases */
#define LED_PIN     GPIO_PIN_13
#define LED_PORT    GPIOC
