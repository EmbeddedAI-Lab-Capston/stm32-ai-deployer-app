# STM32 AI Deployer - Template Reference

## Overview

Templates are copied and expanded by `TemplateEngine` during the Pipeline Wizard.
Every `{{PLACEHOLDER}}` token in file contents and file paths is replaced with the
value entered in the wizard.

## Supported Template Status

| Board | Status | Notes |
| --- | --- | --- |
| STM32F4 | Ready | Base project, startup, linker, HAL glue and Makefile are present. |
| STM32H7 | Ready | Base project, startup, linker, HAL glue and Makefile are present. |
| STM32N6 | Experimental | Placeholder template only. Full startup/linker/HAL files still need board-specific validation. |

## Available Placeholders

| Token | Example value | Description |
| --- | --- | --- |
| `{{MODEL_NAME}}` | `MLP_INT8` | Model base name |
| `{{BOARD_NAME}}` | `STM32F4` | Target board identifier |
| `{{SENSOR_TYPE}}` | `MPU6050` | Sensor class name |
| `{{SENSOR_TYPE_LOWER}}` | `mpu6050` | Lowercase sensor name |
| `{{I2C_INSTANCE}}` | `I2C1` | I2C peripheral instance |
| `{{SDA_PORT}}` | `GPIOB` | SDA GPIO port |
| `{{SDA_PIN}}` | `GPIO_PIN_9` | SDA GPIO pin |
| `{{SCL_PORT}}` | `GPIOB` | SCL GPIO port |
| `{{SCL_PIN}}` | `GPIO_PIN_8` | SCL GPIO pin |
| `{{I2C_ADDRESS}}` | `0xD0` | I2C device address |
| `{{SAI_INSTANCE}}` | `SAI1` | SAI peripheral instance |
| `{{CLK_PORT}}` | `GPIOB` | SAI CLK port |
| `{{CLK_PIN}}` | `GPIO_PIN_5` | SAI CLK pin |
| `{{DATA_PORT}}` | `GPIOB` | SAI DATA port |
| `{{DATA_PIN}}` | `GPIO_PIN_3` | SAI DATA pin |

## Directory Structure

```text
templates/
├── base/
│   ├── STM32F4/   - Cortex-M4, HAL F4, FPU hard-float
│   ├── STM32H7/   - Cortex-M7, HAL H7
│   └── STM32N6/   - Cortex-M55/NPU, experimental
├── sensors/
│   ├── MPU6050/   - I2C 6-DoF IMU
│   ├── BME280/    - I2C environmental sensor
│   └── PDM_MIC/   - SAI PDM microphone
└── ai_glue/
    ├── ai_runner.c / .h
    └── uart_report.c / .h
```

## UART Reporting

The `ai_glue/uart_report.*` helper emits Protocol v1 packets for boot, inference,
system status, benchmark, error and real sensor records. Use `UART_Report_Sensor`
after a real sensor frame is captured so the desktop app can populate the
**Gerçek Sensör Veri Analizleri** table.

## Prerequisites

To compile a generated project you need:

- `arm-none-eabi-gcc` 13.x or newer.
- GNU Make.
- STM32CubeF4, STM32CubeH7 or STM32CubeN6 firmware package for the target board.

Set `CUBE_SDK_PATH` or select the SDK directory in Pipeline Wizard. The Makefile's
`GCC_PATH` variable is set automatically by `PipelineRunner`.
