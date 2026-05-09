#pragma once

#include "stm32h7xx_hal.h"
#include "ai_config.h"

void Error_Handler(void);

/* NUCLEO-H723ZG LD1 */
#define LED_PIN     GPIO_PIN_0
#define LED_PORT    GPIOB
