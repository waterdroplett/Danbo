#ifndef CPU_H
#define CPU_H

#include <stdint.h>

// Disable interrupts and return the previous flags so the caller can restore them.
static inline uint32_t irq_save(void) {
    uint32_t flags;
    __asm__ volatile ("pushf; pop %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static inline void irq_restore(uint32_t flags) {
    __asm__ volatile ("push %0; popf" : : "r"(flags) : "memory", "cc");
}

#endif