/* stm32n6xx_hal_msp.c - low-level HAL peripheral init for STM32N657x0-DK */

#include "main.h"

extern UART_HandleTypeDef hlpuart1;
extern I2C_HandleTypeDef  hi2c1;

void HAL_MspInit(void)
{
    __HAL_RCC_SYSCFG_CLK_ENABLE();
}

/* ── USART1 — ST-Link VCP: TX=PE5, RX=PE6 ──────────────────────────────── */
void HAL_UART_MspInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        RCC_PeriphCLKInitTypeDef periphClk = {0};
        periphClk.PeriphClockSelection = RCC_PERIPHCLK_LPUART1;
        periphClk.Lpuart1ClockSelection = RCC_LPUART1CLKSOURCE_HSI;
        if (HAL_RCCEx_PeriphCLKConfig(&periphClk) != HAL_OK) {
            Error_Handler();
        }

        __HAL_RCC_LPUART1_CLK_ENABLE();
        __HAL_RCC_GPIOE_CLK_ENABLE();
        HAL_GPIO_ConfigPinAttributes(GPIOE, GPIO_PIN_5 | GPIO_PIN_6, GPIO_PIN_SEC | GPIO_PIN_NPRIV);

        GPIO_InitTypeDef gpio = {0};
        gpio.Pin       = GPIO_PIN_5 | GPIO_PIN_6;
        gpio.Mode      = GPIO_MODE_AF_PP;
        gpio.Pull      = GPIO_PULLUP;
        gpio.Speed     = GPIO_SPEED_FREQ_VERY_HIGH;
        gpio.Alternate = GPIO_AF3_LPUART1;
        HAL_GPIO_Init(GPIOE, &gpio);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef *huart)
{
    if (huart->Instance == LPUART1) {
        __HAL_RCC_LPUART1_FORCE_RESET();
        __HAL_RCC_LPUART1_RELEASE_RESET();
        HAL_GPIO_DeInit(GPIOE, GPIO_PIN_5 | GPIO_PIN_6);
    }
}

/* ── I2C — sensor bus ───────────────────────────────────────────────────── */
void HAL_I2C_MspInit(I2C_HandleTypeDef *hi2c)
{
    if (hi2c->Instance == {{I2C_INSTANCE}}) {
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
