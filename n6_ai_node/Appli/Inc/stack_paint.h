#pragma once

#include <stdint.h>

/* Paints the unused part of the stack with a known pattern so the host can
 * compute a high-water mark by scanning for the first untouched byte.
 * Without this, nothing pre-fills the stack and watermark is impossible.
 * Called once from main(), before any deep call. Paints from _sstack (all
 * three board linker scripts define it) up to the current SP minus a
 * safety margin, so the live frame is never touched.
 *
 * Only meaningful for firmware built by this pipeline — a stack region
 * painted by a different toolchain/linker script will not match this
 * pattern, and the host-side watermark reading must show "unavailable"
 * rather than a fabricated number in that case.
 */
#define STACK_PAINT_PATTERN 0xA5A5A5A5u
#define STACK_PAINT_MARGIN  128u

void     StackPaint_Init(void);
uint32_t StackPaint_Pattern(void);     /* keeps the symbol referenced */
