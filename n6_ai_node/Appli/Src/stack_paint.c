/* stack_paint.c — fills unused stack RAM with a known pattern at boot so the
 * host-side Variable Watcher can compute a high-water mark (Faz 8) by
 * scanning down from _sstack for the first word that no longer matches
 * STACK_PAINT_PATTERN.
 */
#include "stack_paint.h"
#include "main.h"

/* Provided by the linker script (bottom of the reserved stack region,
 * _estack - _Min_Stack_Size — all three board .ld files define this). */
extern uint32_t _sstack;

void StackPaint_Init(void)
{
    uint32_t sp = __get_MSP();
    uint32_t *start = &_sstack;
    uint32_t *end   = (uint32_t *)(sp - STACK_PAINT_MARGIN);

    if (end <= start)
        return;   /* nothing safe to paint — leave stack untouched */

    for (uint32_t *p = start; p < end; ++p) {
        *p = STACK_PAINT_PATTERN;
    }
}

uint32_t StackPaint_Pattern(void)
{
    return STACK_PAINT_PATTERN;
}
