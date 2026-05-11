#pragma once

#include "main.h"
#include <stdint.h>

/* I2C address: AD0=LOW → 0xD0, AD0=HIGH → 0xD2 */
#define MPU6050_I2C_ADDR    {{I2C_ADDRESS}}
#define MPU6050_HAL_ADDR    ((MPU6050_I2C_ADDR < 0x80U) ? (MPU6050_I2C_ADDR << 1) : MPU6050_I2C_ADDR)

/* Register map */
#define MPU6050_REG_PWR_MGMT_1   0x6B
#define MPU6050_REG_SMPLRT_DIV   0x19
#define MPU6050_REG_CONFIG       0x1A
#define MPU6050_REG_GYRO_CONFIG  0x1B
#define MPU6050_REG_ACCEL_CONFIG 0x1C
#define MPU6050_REG_ACCEL_XOUT_H 0x3B
#define MPU6050_REG_WHO_AM_I     0x75

/* Scaling — ±2g, ±250°/s */
#define MPU6050_ACCEL_SCALE   (9.81f / 16384.0f)  /* m/s² per LSB */
#define MPU6050_GYRO_SCALE    (1.0f  / 131.0f)    /* °/s  per LSB */

/* Sensor reading: ax, ay, az, gx, gy, gz (all in normalised floats) */
typedef struct {
    float ax, ay, az;
    float gx, gy, gz;
} MPU6050_Data;

HAL_StatusTypeDef MPU6050_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef MPU6050_ReadAll(I2C_HandleTypeDef *hi2c, MPU6050_Data *data);

/* Sensor API used by main.c template */
void Sensor_Init(I2C_HandleTypeDef *hi2c);
HAL_StatusTypeDef Sensor_Read(float *out, uint16_t len);
