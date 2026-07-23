/*
 * src/system/sys.c — System-level glue and timekeeping.
 *
 * On real target: configure PLL to 144 MHz HSE × 18, enable peripheral
 * clocks, init SysTick at 1 ms.
 * On host tests: just zero a counter.
 */
#include "sayofw.h"
#include "sayofw_config.h"

#include <stdint.h>

#ifndef HOST_TESTING
/* Real-target path: pull vendor prototypes. The vendor SDK exposes these
 * via "ch32v30x.h". For the host test build we skip the include (no SDK). */
#define HOST_TESTING 0
#endif

static volatile uint32_t g_tick_ms = 0U;

void sys_init(void)
{
#if HOST_TESTING
    g_tick_ms = 0U;
#else
    /* On target, fill in: HSE on → PLL × 18 → SYSCLK 144 MHz, then
     * SysTick_Config(SystemCoreClock / 1000). Left as a TODO for the
     * integration phase. */
    g_tick_ms = 0U;
#endif
}

/* SysTick ISR prototype. Weak, may be overridden by SysTick handler from
 * the vendor HAL when integrated. */
void SysTick_Handler(void);
void SysTick_Handler(void)
{
    g_tick_ms++;
}

uint32_t sys_tick_ms(void)
{
    return g_tick_ms;
}
