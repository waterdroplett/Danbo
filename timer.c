// timer.c
#include "timer.h"
#include "irq.h"
#include "io.h"
#include "task.h"

static volatile uint32_t ticks = 0;

static void timer_callback(struct registers *r) {
    (void)r;
    ticks++;
    task_tick();
}

void timer_init(uint32_t hz) {
    uint32_t divisor = 1193182 / hz;
    outb(0x43, 0x36);                    // channel 0, square wave
    outb(0x40, divisor & 0xFF);
    outb(0x40, (divisor >> 8) & 0xFF);
    irq_install_handler(0, timer_callback);
}

uint32_t timer_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t t) {
    uint32_t end = ticks + t;
    while ((int32_t)(ticks - end) < 0)
        __asm__ volatile ("hlt");        // sleep until the next interrupt
}