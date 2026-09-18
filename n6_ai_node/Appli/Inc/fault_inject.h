#pragma once

/* Deliberate faults for demonstrating the STM32 AI Deployer's diagnosis
 * (docs/n6_ai_reference_project.md, "Hata enjeksiyonu").
 *
 * Every switch defaults to 0 - the shipped build is the healthy one. To build
 * a faulty variant, set exactly one to 1 and rebuild. Nothing in the firmware
 * reports which fault is active: finding it is the tool's job.
 *
 * FAULT_I2C1_CLOCK_OFF
 *   The I2C1 MSP "forgets" __HAL_RCC_I2C1_CLK_ENABLE(). A real, common bug:
 *   writes to an unclocked peripheral are silently dropped, HAL_I2C_Init()
 *   still returns HAL_OK, the sensor reads time out, and the NPU keeps
 *   running - the board looks alive. Expected diagnosis: the Variable
 *   Watcher shows the sensor failing; the Register Inspector shows
 *   RCC_APB1LENR.I2C1EN = 0 and I2C1 still at its reset values while the
 *   pins are already configured for it.
 */
#define FAULT_I2C1_CLOCK_OFF  0
