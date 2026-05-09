#pragma once

#include <stdint.h>

/* Inference result from a single forward pass */
typedef struct {
    uint8_t  class_id;         /* Argmax class index */
    uint8_t  confidence_pct;   /* 0-100 */
    char     label[24];        /* Human-readable class label */
} AI_InferenceResult;

/* Initialise the X-CUBE-AI network (call once after HAL init) */
void     AI_Runner_Init(void);

/* Run one forward pass.
 * input  : pointer to flat float32 input array (length = AI_INPUT_SIZE)
 * result : filled with class and confidence
 * Returns elapsed time in microseconds (measured via DWT or TIM).
 */
uint32_t AI_Runner_Infer(const float *input, AI_InferenceResult *result);

/* Memory usage helpers — values valid after first call to AI_Runner_Infer */
uint32_t AI_Runner_GetRamUsage(void);   /* bytes used by activations */
uint32_t AI_Runner_GetFreeRam(void);    /* approximate heap free bytes */
