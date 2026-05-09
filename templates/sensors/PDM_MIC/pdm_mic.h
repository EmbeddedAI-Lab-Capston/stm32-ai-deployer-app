#pragma once

#include "stm32f4xx_hal.h"
#include <stdint.h>

/* SAI configuration — PDM microphone */
#define PDM_MIC_SAI_INSTANCE   {{SAI_INSTANCE}}
#define PDM_MIC_CLK_PORT       {{CLK_PORT}}
#define PDM_MIC_CLK_PIN        {{CLK_PIN}}
#define PDM_MIC_DATA_PORT      {{DATA_PORT}}
#define PDM_MIC_DATA_PIN       {{DATA_PIN}}

/* PCM buffer size (16-bit samples, 16 kHz, 1 channel) */
#define PDM_BUFFER_SIZE     256
#define PCM_SAMPLE_RATE     16000
#define PCM_CHANNELS        1

/* FFT / MFCC feature vector length */
#define MFCC_NUM_COEFFS     40
#define MFCC_FRAME_LEN      512

typedef struct {
    int16_t pcm[PDM_BUFFER_SIZE];
    float   mfcc[MFCC_NUM_COEFFS];
} PDM_AudioFrame;

HAL_StatusTypeDef PDM_Mic_Init(SAI_HandleTypeDef *hsai);
HAL_StatusTypeDef PDM_Mic_Capture(SAI_HandleTypeDef *hsai, PDM_AudioFrame *frame);
void              PDM_Mic_ComputeMFCC(const int16_t *pcm, float *mfcc, uint16_t len);

/* Sensor API used by main.c template */
void Sensor_Init(I2C_HandleTypeDef *hi2c);  /* hi2c unused for PDM; kept for API compat */
void Sensor_Read(float *out, uint16_t len);
