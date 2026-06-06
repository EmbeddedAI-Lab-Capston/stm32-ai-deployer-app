/* stm32h7xx_hal_msp.c - low-level HAL init for NUCLEO-H723ZG */

#include "main.h"

extern UART_HandleTypeDef huart3;
extern I2C_HandleTypeDef  hi2c1;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
    HAL_NVIC_SetPriorityGrouping(NVIC_PRIORITYGROUP_4);
}

void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_8 | GPIO_PIN_9; /* ST-LINK VCP: TX=PD8, RX=PD9 */
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_NOPULL;
        gpio.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOD, &gpio);

        HAL_NVIC_SetPriority(USART3_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART3) {
        __HAL_RCC_USART3_FORCE_RESET();
        __HAL_RCC_USART3_RELEASE_RESET();
        HAL_GPIO_DeInit(GPIOD, GPIO_PIN_8 | GPIO_PIN_9);
        HAL_NVIC_DisableIRQ(USART3_IRQn);
    }
}

void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == {{I2C_INSTANCE}}) {
        RCC_PeriphCLKInitTypeDef periph_clk = {0};
        periph_clk.PeriphClockSelection = RCC_PERIPHCLK_I2C1;
        periph_clk.I2c123ClockSelection = RCC_I2C1235CLKSOURCE_D2PCLK1;
        if (HAL_RCCEx_PeriphCLKConfig(&periph_clk) != HAL_OK) {
            Error_Handler();
        }

        __HAL_RCC_{{I2C_INSTANCE}}_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = {{SCL_PIN}} | {{SDA_PIN}};
        gpio.Mode      = GPIO_MODE_AF_OD;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_LOW;
        gpio.Alternate = GPIO_AF4_I2C1;
        HAL_GPIO_Init({{SCL_PORT}}, &gpio);
    }
}

void HAL_I2C_MspDeInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == {{I2C_INSTANCE}}) {
        __HAL_RCC_{{I2C_INSTANCE}}_FORCE_RESET();
        __HAL_RCC_{{I2C_INSTANCE}}_RELEASE_RESET();
        HAL_GPIO_DeInit({{SCL_PORT}}, {{SCL_PIN}} | {{SDA_PIN}});
    }
}
