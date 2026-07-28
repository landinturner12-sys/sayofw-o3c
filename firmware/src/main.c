/*
 * src/main.c — Bare-metal main loop.
 *
 *   1. sys_init()       — clock tree
 *   2. display_init()   — compositor + OLED driver
 *   3. protocol_init()  — bind rx callback, register dispatcher
 *   4. keys_init()      — scan GPIOs
 *
 *   loop:
 *     - protocol_pump()        drain pending HID packets
 *     - keys_scan()            debounce + report
 *     - compositor_flush()     push dirty rect to OLED
 *     - __WFI()                sleep until next interrupt
 */
#include "sayofw.h"
#include "sayofw_config.h"

#include <stdint.h>

int main(void)
{
    sys_init();

    /* display_init must run BEFORE protocol_init: it calls
     * protocol_set_compositor(&g_compositor). Any HID packet that
     * arrives between protocol_init and display_init would otherwise
     * be dispatched with a NULL compositor and silently no-op display
     * commands. */
    extern void display_init(void);
    extern void display_flush(void);
    display_init();

    /* Now safe to wire the protocol dispatcher — it can find g_compositor. */
    extern void protocol_init(void);
    extern void protocol_pump(void);
    protocol_init();

    extern void keys_init(key_state_t *s);
    extern void keys_scan(key_state_t *s);
    key_state_t keys;
    keys_init(&keys);

    for (;;) {
        protocol_pump();
        keys_scan(&keys);
        display_flush();
        /* __WFI() once we wire NVIC. For now, busy-loop. */
    }
}
