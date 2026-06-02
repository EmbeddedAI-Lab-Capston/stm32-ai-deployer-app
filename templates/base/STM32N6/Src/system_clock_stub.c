#include <stdint.h>

extern uint32_t SystemCoreClock;

uint32_t SECURE_SystemCoreClockUpdate(void)
{
    return SystemCoreClock;
}
