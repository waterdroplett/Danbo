#ifndef IDT_H
#define IDT_H

#include <stdint.h>

// Must match the push order in isr_stubs.asm
struct registers {
    uint32_t ds;
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax;
    uint32_t int_no, err_code;
    uint32_t eip, cs, eflags;
};

void idt_init(void);

#endif