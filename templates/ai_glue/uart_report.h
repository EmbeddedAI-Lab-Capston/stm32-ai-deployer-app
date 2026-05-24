#pragma once

#include <stdint.h>

/* Protocol v1.0 — § prefix, JSON body, \r\n terminator
 * All functions use HAL_UART_Transmit_DMA (non-blocking).
 * Call UART_Report_SetHandle() before any other function.
 */

struct UART_HandleTypeDef;  /* forward-declare to avoid full include */

void UART_Report_SetHandle(void *huart);

/* {"t":"boot","card":"...","sdk":"...","model":"...","baud":115200} */
void UART_Report_Boot(const char *model,
                      const char *card,
                      const char *sdk,
                      uint32_t    baud);

/* {"t":"inf","model":"...","inf_us":N,"ram_b":N,"acc_pct":N,"label":"...","card":"..."} */
void UART_Report_Inference(const char *model,
                           uint32_t    inf_us,
                           uint32_t    ram_b,
                           uint8_t     acc_pct,
                           const char *label,
                           const char *card);

/* {"t":"sys","uptime_s":N,"temp_c":N,"free_ram_b":N,"state":"..."} */
void UART_Report_SysStatus(uint32_t    uptime_s,
                           int32_t     temp_c,
                           uint32_t    free_ram_b,
                           const char *state);

/* {"t":"err","code":N,"msg":"..."} */
void UART_Report_Error(uint32_t code, const char *msg);

/* {"t":"bench","model":"...","samples":N,"avg_us":N,"min_us":N,"max_us":N,"ram_b":N,"free_ram_b":N,"label":"...","card":"..."} */
void UART_Report_Benchmark(const char *model,
                           uint32_t samples,
                           uint32_t avg_us,
                           uint32_t min_us,
                           uint32_t max_us,
                           uint32_t ram_b,
                           uint32_t free_ram_b,
                           const char *label,
                           const char *card);
