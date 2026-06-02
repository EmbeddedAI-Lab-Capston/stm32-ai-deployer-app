#pragma once

#include <QString>

// Complete configuration for a .tflite → compile → flash pipeline run.
// Populated page-by-page in PipelineWizard and consumed by PipelineRunner.
struct PipelineConfig {
    // ── Step 1: AI model ─────────────────────────────────────────────────
    QString modelPath;
    QString modelName;
    QString architecture;   // "MLP" / "1D CNN" / "LSTM"
    QString quantization;   // "INT8" / "Float32" / "Dynamic Q"

    // ── Step 2: Sensor ────────────────────────────────────────────────────
    QString sensorType;     // "MPU6050" / "BME280" / "PDM_MIC"
    QString protocol;       // "I2C" / "SAI"
    QString i2cInstance;    // "I2C1"
    QString sdaPort;        // "GPIOB"
    QString sdaPin;         // "GPIO_PIN_9"
    QString sclPort;        // "GPIOB"
    QString sclPin;         // "GPIO_PIN_8"
    QString i2cAddress;     // "0x76"
    QString saiInstance;    // "SAI1"
    QString clkPort;        // "GPIOB"
    QString clkPin;         // "GPIO_PIN_5"
    QString dataPort;       // "GPIOB"
    QString dataPin;        // "GPIO_PIN_3"

    // ── Step 3: Tools & output ────────────────────────────────────────────
    QString targetBoard;         // "STM32F4" / "STM32H7" / "STM32N6"
    QString gccPath;
    QString makePath;
    QString xcubeCliPath;
    QString programmerCliPath;
    QString outputDir;
    QString cubeSdkPath;         // STM32Cube_FW_Fx root; auto-detected if empty
};
