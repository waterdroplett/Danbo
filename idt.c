#include <stdint.h>
#include "idt.h"
#include "term.h"
#include "irq.h"

struct idt_entry {
    uint16_t base_low;
    uint16_t sel;
    uint8_t  zero;
    uint8_t  flags;
    uint16_t base_high;
} __attribute__((packed));

struct idt_ptr {
    uint16_t limit;
    uint32_t base;
} __attribute__((packed));



extern uint32_t isr_stub_table[32];
extern uint32_t irq_stub_table[16];

static struct idt_entry idt[256];
static struct idt_ptr ip;

static const char *exception_names[32] = {
    "Divide Error", "Debug", "Non-Maskable Interrupt", "Breakpoint",
    "Overflow", "Bound Range Exceeded", "Invalid Opcode", "Device Not Available",
    "Double Fault", "Coprocessor Segment Overrun", "Invalid TSS", "Segment Not Present",
    "Stack-Segment Fault", "General Protection Fault", "Page Fault", "Reserved",
    "x87 Floating-Point", "Alignment Check", "Machine Check", "SIMD Floating-Point",
    "Virtualization", "Control Protection", "Reserved", "Reserved",
    "Reserved", "Reserved", "Reserved", "Reserved",
    "Hypervisor Injection", "VMM Communication", "Security Exception", "Reserved"
};

static void idt_set_gate(int num, uint32_t base, uint16_t sel, uint8_t flags) {
    idt[num].base_low  = base & 0xFFFF;
    idt[num].base_high = (base >> 16) & 0xFFFF;
    idt[num].sel       = sel;
    idt[num].zero      = 0;
    idt[num].flags     = flags;
}

void idt_init(void) {
    ip.limit = sizeof(idt) - 1;
    ip.base  = (uint32_t)&idt;

    // 0x8E = present, ring 0, 32-bit interrupt gate. 0x08 = kernel code selector.
    for (int i = 0; i < 32; i++)
        idt_set_gate(i, isr_stub_table[i], 0x08, 0x8E);

    for (int i = 0; i < 16; i++)
      idt_set_gate(32 + i, irq_stub_table[i], 0x08, 0x8E);
    __asm__ volatile ("lidt %0" : : "m"(ip));
}

void isr_handler(struct registers *r) {
    if (r->int_no >= 32) {
        irq_dispatch(r);
        return;
    }
    term_set_color(15, 4); // white on red
    term_print("\n*** CPU EXCEPTION: ");
    if (r->int_no < 32)
        term_print(exception_names[r->int_no]);
    term_print(" ***\n");

    term_print("INT:    "); term_print_dec(r->int_no);  term_putc('\n');
    term_print("ERR:    "); term_print_hex(r->err_code); term_putc('\n');
    term_print("EIP:    "); term_print_hex(r->eip);      term_putc('\n');
    term_print("CS:     "); term_print_hex(r->cs);       term_putc('\n');
    term_print("EFLAGS: "); term_print_hex(r->eflags);   term_putc('\n');
    term_print("EAX:    "); term_print_hex(r->eax);      term_putc('\n');
    term_print("EBX:    "); term_print_hex(r->ebx);      term_putc('\n');
    term_print("ECX:    "); term_print_hex(r->ecx);      term_putc('\n');
    term_print("EDX:    "); term_print_hex(r->edx);      term_putc('\n');

    if (r->int_no == 14) {
        uint32_t cr2;
        __asm__ volatile ("mov %%cr2, %0" : "=r"(cr2));   // CPU stores the bad address here
        term_print("\nFaulting address: "); term_print_hex(cr2); term_putc('\n');
        term_print("Cause: ");
        term_print(r->err_code & 1 ? "protection violation" : "page not present");
        term_print(r->err_code & 2 ? ", write" : ", read");
        term_print(r->err_code & 4 ? ", user mode" : ", kernel mode");
        term_putc('\n');
    }
    
    term_print("\nSystem halted.\n");
    for (;;) __asm__ volatile ("cli; hlt");
}