#pragma once

/* ── N6 HAL configuration ─────────────────────────────────────────────────────
 * stm32n6xx_hal.h includes ONLY this conf file and nothing else.
 * Therefore this file MUST include every module header we need.
 * The module headers include stm32n6xx_hal_def.h which in turn includes
 * stm32n6xx.h → core_cm55.h, defining __IO, uint32_t, HAL_StatusTypeDef, etc.
 *
 * Do NOT define USE_HAL_DRIVER here — already passed by Makefile -DUSE_HAL_DRIVER.
 * ─────────────────────────────────────────────────────────────────────────── */

/* ── Oscillator values ────────────────────────────────────────────────────── */
#if !defined(HSE_VALUE)
  #define HSE_VALUE             48000000U   /* STM32N657x0-DK: 48 MHz HSE */
#endif
#if !defined(HSI_VALUE)
  #define HSI_VALUE             64000000U   /* HSI: 64 MHz */
#endif
#if !defined(HSE_STARTUP_TIMEOUT)
  #define HSE_STARTUP_TIMEOUT   100U
#endif
#if !defined(LSE_VALUE)
  #define LSE_VALUE             32768U
#endif
#if !defined(LSE_STARTUP_TIMEOUT)
  #define LSE_STARTUP_TIMEOUT   5000U
#endif
#if !defined(VDD_VALUE)
  #define VDD_VALUE             3300U       /* mV */
#endif

/* ── SysTick priority ─────────────────────────────────────────────────────── */
#define TICK_INT_PRIORITY       0x0FU
#define USE_RTOS                0U

#define assert_param(expr) ((void)0U)

/* ── Module enables ───────────────────────────────────────────────────────── */
#define HAL_MODULE_ENABLED
#define HAL_GPIO_MODULE_ENABLED
#define HAL_RCC_MODULE_ENABLED
#define HAL_CORTEX_MODULE_ENABLED
#define HAL_DMA_MODULE_ENABLED
#define HAL_UART_MODULE_ENABLED
#define HAL_I2C_MODULE_ENABLED
#define HAL_TIM_MODULE_ENABLED
#define HAL_PWR_MODULE_ENABLED
/* NOTE: HAL_FLASH_MODULE_ENABLED intentionally omitted —
 *       flash is handled by STM32_Programmer_CLI, not HAL.             */

/* ── Module headers ───────────────────────────────────────────────────────────
 * N6 HAL expects the conf to pull in all required module headers.
 * stm32n6xx_hal_rcc.h → stm32n6xx_hal_def.h → stm32n6xx.h → CMSIS types.
 * ─────────────────────────────────────────────────────────────────────────── */
#ifdef HAL_RCC_MODULE_ENABLED
  #include "stm32n6xx_hal_rcc.h"
#endif

#ifdef HAL_GPIO_MODULE_ENABLED
  #include "stm32n6xx_hal_gpio.h"
#endif

#ifdef HAL_DMA_MODULE_ENABLED
  #include "stm32n6xx_hal_dma.h"
#endif

#ifdef HAL_CORTEX_MODULE_ENABLED
  #include "stm32n6xx_hal_cortex.h"
#endif

#ifdef HAL_PWR_MODULE_ENABLED
  #include "stm32n6xx_hal_pwr.h"
#endif

#ifdef HAL_UART_MODULE_ENABLED
  #include "stm32n6xx_hal_uart.h"
#endif

#ifdef HAL_I2C_MODULE_ENABLED
  #include "stm32n6xx_hal_i2c.h"
#endif

#ifdef HAL_TIM_MODULE_ENABLED
  #include "stm32n6xx_hal_tim.h"
#endif
