#include <stdint.h>
#include "gdt.h"

struct gdt_entry {
    uint16_t limit_low;
    uint16_t base_low;
    uint8_t  base_mid;
    uint8_t  access;
    uint8_t  granularity;
    uint8_t  base_high;
} __attribute__((packed));

struct gdt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));

static struct gdt_entry gdt[5];
static struct gdt_ptr gp;

static void gdt_set(int i, uint32_t base, uint32_t limit,
                    uint8_t access, uint8_t gran) {
    gdt[i].base_low    = base & 0xFFFF;
    gdt[i].base_mid    = (base >> 16) & 0xFF;
    gdt[i].base_high   = (base >> 24) & 0xFF;
    gdt[i].limit_low   = limit & 0xFFFF;
    gdt[i].granularity = ((limit >> 16) & 0x0F) | (gran & 0xF0);
    gdt[i].access      = access;
}

void gdt_init(void) {
    gp.limit = sizeof(gdt) - 1;
    gp.base  = (uint32_t)&gdt;

    gdt_set(0, 0, 0,       0x00, 0x00); // null descriptor (required)
    gdt_set(1, 0, 0xFFFFF, 0x9A, 0xCF); // kernel code  (selector 0x08)
    gdt_set(2, 0, 0xFFFFF, 0x92, 0xCF); // kernel data  (selector 0x10)
    gdt_set(3, 0, 0xFFFFF, 0xFA, 0xCF); // user code    (selector 0x18)
    gdt_set(4, 0, 0xFFFFF, 0xF2, 0xCF); // user data    (selector 0x20)

    __asm__ volatile (
        "lgdt %0\n"
        "mov $0x10, %%ax\n"
        "mov %%ax, %%ds\n"
        "mov %%ax, %%es\n"
        "mov %%ax, %%fs\n"
        "mov %%ax, %%gs\n"
        "mov %%ax, %%ss\n"
        "pushl $0x08\n"      // reload CS with a far return
        "pushl $1f\n"
        "lret\n"
        "1:\n"
        : : "m"(gp) : "ax", "memory"
    );
}