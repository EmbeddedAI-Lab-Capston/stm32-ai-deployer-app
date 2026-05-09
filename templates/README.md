# STM32 AI Deployer — Template Reference

## Overview

Templates are copied and expanded by **TemplateEngine** during the Pipeline Wizard.
Every `{{PLACEHOLDER}}` token in file contents and file paths is replaced with the
value the user entered in the wizard.

## Available Placeholders

| Token              | Example value       | Description                |
|--------------------|---------------------|----------------------------|
| `{{MODEL_NAME}}`   | `MLP_INT8`          | Model base name            |
| `{{BOARD_NAME}}`   | `STM32F4`           | Target board identifier    |
| `{{SENSOR_TYPE}}`  | `MPU6050`           | Sensor class name          |
| `{{SENSOR_TYPE_LOWER}}` | `mpu6050`      | Lowercase sensor name      |
| `{{I2C_INSTANCE}}` | `I2C1`              | I2C peripheral instance    |
| `{{SDA_PORT}}`     | `GPIOB`             | SDA GPIO port              |
| `{{SDA_PIN}}`      | `GPIO_PIN_7`        | SDA GPIO pin               |
| `{{SCL_PORT}}`     | `GPIOB`             | SCL GPIO port              |
| `{{SCL_PIN}}`      | `GPIO_PIN_6`        | SCL GPIO pin               |
| `{{I2C_ADDRESS}}`  | `0xD0`              | I2C device address (8-bit) |
| `{{SAI_INSTANCE}}` | `SAI1`              | SAI peripheral instance    |
| `{{CLK_PORT}}`     | `GPIOB`             | SAI CLK port               |
| `{{CLK_PIN}}`      | `GPIO_PIN_5`        | SAI CLK pin                |
| `{{DATA_PORT}}`    | `GPIOB`             | SAI DATA port              |
| `{{DATA_PIN}}`     | `GPIO_PIN_3`        | SAI DATA pin               |

## Directory Structure

```
templates/
├── base/
│   ├── STM32F4/   — Cortex-M4, HAL F4, FPU hard-float
│   ├── STM32H7/   — Cortex-M7, HAL H7, dual-bank flash
│   └── STM32N6/   — Cortex-M55, HAL N6, NPU
├── sensors/
│   ├── MPU6050/   — I2C 6-DoF IMU (HAR)
│   ├── BME280/    — I2C environmental sensor
│   └── PDM_MIC/   — SAI PDM microphone (KWS)
└── ai_glue/
    ├── ai_runner.c / .h    — X-CUBE-AI inference wrapper
    └── uart_report.c / .h  — Protocol v1.0 UART reporting
```

## Prerequisites

To actually compile the generated project you need:

- **arm-none-eabi-gcc** ≥ 13.x (comes with STM32CubeIDE)
- **GNU Make** (comes with STM32CubeIDE)
- **STM32CubeF4 / H7 / N6** SDK installed so the Makefile can find HAL headers.
  Set the `CUBE_SDK_PATH` environment variable or adjust the `Makefile` directly.

The Makefile's `GCC_PATH` variable is set automatically by PipelineRunner.
