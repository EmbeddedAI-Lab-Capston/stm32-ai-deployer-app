#pragma once

#include "main.h"
#include <stdint.h>

/* I2C address: SDO=LOW → 0xEC, SDO=HIGH → 0xEE */
#define BME280_I2C_ADDR    {{I2C_ADDRESS}}
#define BME280_HAL_ADDR    ((BME280_I2C_ADDR < 0x80U) ? (BME280_I2C_ADDR << 1) : BME280_I2C_ADDR)

#define BME280_REG_CHIP_ID    0xD0
#define BME280_REG_RESET      0xE0
#define BME280_REG_CTRL_HUM   0xF2
#define BME280_REG_STATUS     0xF3
#define BME280_REG_CTRL_MEAS  0xF4
#define BME280_REG_CONFIG     0xF5
#define BME280_REG_PRESS_MSB  0xF7
#define BME280_CHIP_ID_VAL    0x60

typedef struct {
    float temperature_c;
    float pressure_hpa;
    float humidity_pct;
} BME280_Data;

HAL_StatusTypeDef BME280_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef BME280_ReadAll(I2C_HandleTypeDef *hi2c, BME280_Data *data);

/* Sensor API used by main.c template */
void Sensor_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef Sensor_Read(float *out, uint16_t len);
