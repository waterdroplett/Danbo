#include <stdint.h>
#include <stddef.h>
#include "gdt.h"
#include "idt.h"
#include "term.h"
#include "pic.h"
#include "timer.h"
#include "keyboard.h"
#include "pmm.h"
#include "multiboot.h"
#include "paging.h"
#include "string.h"
#include "heap.h"
#include "shell.h"
#include "mouse.h"
#include "cpu.h"
#include "task.h"

#define VGA_WIDTH  80
#define VGA_HEIGHT 25
#define VGA_MEMORY ((volatile uint16_t*)0xB8000)

static size_t row = 0, col = 0;
static uint8_t color = 0x0F; // white on black

static inline uint16_t vga_entry(char c, uint8_t clr) {
    return (uint16_t)(uint8_t)c | ((uint16_t)clr << 8);
}

void term_clear(void) {
    for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++)
        VGA_MEMORY[i] = vga_entry(' ', color);
    row = col = 0;
}

static void term_scroll(void) {
    for (size_t y = 1; y < VGA_HEIGHT; y++)
        for (size_t x = 0; x < VGA_WIDTH; x++)
            VGA_MEMORY[(y - 1) * VGA_WIDTH + x] = VGA_MEMORY[y * VGA_WIDTH + x];
    for (size_t x = 0; x < VGA_WIDTH; x++)
        VGA_MEMORY[(VGA_HEIGHT - 1) * VGA_WIDTH + x] = vga_entry(' ', color);
    row = VGA_HEIGHT - 1;
}

void term_putc(char c) {
    uint32_t flags = irq_save();
    if (c == '\n') {
        col = 0;
        row++;
    } else if (c == '\r') {
        col = 0;
    } else if (c == '\t') {
        col = (col + 4) & ~3u;
    }
    else if (c == '\b') {
        if (col > 0) {
            col--;
            VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(' ', color);
        }
    } else {
        VGA_MEMORY[row * VGA_WIDTH + col] = vga_entry(c, color);
        col++;
    }
    if (col >= VGA_WIDTH) { col = 0; row++; }
    if (row >= VGA_HEIGHT) term_scroll();
    irq_restore(flags);
}

void term_print(const char *s) {
    uint32_t flags = irq_save();
    while (*s) term_putc(*s++);
    irq_restore(flags);
}

void term_print_hex(uint32_t n) {
    const char *digits = "0123456789ABCDEF";
    term_print("0x");
    for (int shift = 28; shift >= 0; shift -= 4)
        term_putc(digits[(n >> shift) & 0xF]);
}

void term_print_dec(uint32_t n) {
    char buf[11]; // max 10 digits for 32-bit, plus terminator
    int i = 10;
    buf[i] = '\0';
    if (n == 0) {
        term_putc('0');
        return;
    }
    while (n > 0) {
        buf[--i] = '0' + (n % 10);
        n /= 10;
    }
    term_print(&buf[i]);
}

void term_print_int(int32_t n) {
    if (n < 0) {
        term_putc('-');
        term_print_dec((uint32_t)(-(n + 1)) + 1); // safe for INT32_MIN
    } else {
        term_print_dec((uint32_t)n);
    }
}

void term_set_color(uint8_t fg, uint8_t bg) {
    color = fg | (bg << 4);
}

void kernel_main(uint32_t magic, struct multiboot_info *mbi) {
    term_clear();
    term_set_color(10, 0);
    term_print("Danbo booting...\n");
    term_set_color(15, 0);

    if (magic != MULTIBOOT_MAGIC) {
        term_print("Not booted by a Multiboot loader!\n");
        for (;;) __asm__ volatile ("cli; hlt");
    }

    gdt_init();
    idt_init();
    pic_remap();
    timer_init(100);
    keyboard_init();
    mouse_init();
    __asm__ volatile ("sti");

    pmm_init(mbi);
    paging_init();
    heap_init();
    task_init();

    term_print("Danbo Desktop ready. Type 'help'.\n\n");
    shell_run();
}