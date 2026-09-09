#include "telemetry.h"
#include "main.h"     /* for __DMB() (CMSIS core intrinsic) */

volatile TelemetryBlock g_telemetry;

void Telemetry_BeginWrite(void)
{
    g_telemetry.seq_begin++;
    __DMB();                     /* seq visible before the fields that follow */
}

void Telemetry_EndWrite(void)
{
    __DMB();                     /* fields visible before seq_end closes the window */
    g_telemetry.seq_end = g_telemetry.seq_begin;
}
