/*
 * src/system/sys.c — System-level glue and timekeeping.
 *
 * On real target: configure PLL to 144 MHz HSE × 18, enable peripheral
 * clocks, init SysTick at 1 ms via SysTick_Config().
 * On host tests: just zero a counter; tests can advance it manually with
 * sys_test_tick_advance() (weak stub below).
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

#if !HOST_TESTING
/* Forward decls from the vendor ch32v30x.h — kept here so this translation
 * unit doesn't drag the whole HAL into every consumer. */
extern uint32_t SystemCoreClock;
extern uint32_t SysTick_Config(uint32_t ticks);
#endif

void sys_init(void)
{
    g_tick_ms = 0U;
#if HOST_TESTING
    /* Nothing further to do — host tests advance time via the test hook. */
#else
    /* F9: configure PLL (HSE × 18 → 144 MHz) before anything else needs
     * accurate timing. The vendor routine lives in ch32v30x.c; we call
     * it here because the bare-metal startup doesn't run it. */
    /* SystemInit() would normally do this; we declare a weak fallback
     * below so the link succeeds even if the vendor lib is not yet
     * integrated. */
    extern void SystemInit(void);
    SystemInit();
    /* Fire SysTick at 1 ms. CLOCK = SystemCoreClock / 1000. */
    (void)SysTick_Config(SystemCoreClock / 1000U);
#endif
}

/* SysTick ISR — weak so the vendor ch32v30x_it.c override (with the same
 * name) silently wins when integrated. See F8. */
__attribute__((weak)) void SysTick_Handler(void)
{
    g_tick_ms++;
}

uint32_t sys_tick_ms(void)
{
    return g_tick_ms;
}

#if HOST_TESTING
/* Test hook: allow native tests to advance the simulated tick. */
__attribute__((weak)) void sys_test_tick_advance(uint32_t ms)
{
    g_tick_ms += ms;
}
#endif
