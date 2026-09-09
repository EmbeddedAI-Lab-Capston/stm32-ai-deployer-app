#pragma once

#include <stdint.h>

/* Telemetry block read by the host (Variable Watcher) over SWD — no UART
 * required. All fields live in ONE contiguous struct so WatchPlanBuilder
 * merges them into a single memory read, meaning seq_begin and seq_end
 * always arrive from the same SWD snapshot and the seqlock check below is
 * meaningful.
 *
 * Seqlock contract:
 *   writer : seq_begin++ -> DMB -> write fields -> DMB -> seq_end = seq_begin
 *   reader : read the whole block; if seq_begin != seq_end, DISCARD the sample.
 */

#define TELEMETRY_MAX_SENSOR 8
#define TELEMETRY_LABEL_LEN  24

typedef struct {
    volatile uint32_t seq_begin;                       /* offset 0  */
    volatile float    sensor[TELEMETRY_MAX_SENSOR];    /* raw physical value */
    volatile uint32_t sensor_count;                     /* valid entries in sensor[] */
    volatile uint32_t sensor_ok;                        /* 1 = last read succeeded */
    volatile uint32_t inf_us;                           /* last inference duration */
    volatile uint32_t infer_count;                      /* total inferences run */
    volatile uint32_t cycle;                            /* main loop counter */
    volatile uint32_t class_id;
    volatile uint32_t confidence_pct;
    volatile char     label[TELEMETRY_LABEL_LEN];
    volatile uint32_t seq_end;                          /* must equal seq_begin */
} TelemetryBlock;

extern volatile TelemetryBlock g_telemetry;

void Telemetry_BeginWrite(void);
void Telemetry_EndWrite(void);
