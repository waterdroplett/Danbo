// pic.c
#include "pic.h"
#include "io.h"

#define PIC1_CMD  0x20
#define PIC1_DATA 0x21
#define PIC2_CMD  0xA0
#define PIC2_DATA 0xA1

void pic_remap(void) {
    outb(PIC1_CMD, 0x11); io_wait();   // start init sequence
    outb(PIC2_CMD, 0x11); io_wait();
    outb(PIC1_DATA, 0x20); io_wait();  // master IRQs -> vectors 32-39
    outb(PIC2_DATA, 0x28); io_wait();  // slave IRQs  -> vectors 40-47
    outb(PIC1_DATA, 0x04); io_wait();  // slave is on IRQ2
    outb(PIC2_DATA, 0x02); io_wait();
    outb(PIC1_DATA, 0x01); io_wait();  // 8086 mode
    outb(PIC2_DATA, 0x01); io_wait();

    outb(PIC1_DATA, 0xFB);  // mask everything except IRQ2 (the cascade line)
    outb(PIC2_DATA, 0xFF);
}

void pic_unmask(uint8_t irq) {
    uint16_t port = PIC1_DATA;
    if (irq >= 8) { port = PIC2_DATA; irq -= 8; }
    outb(port, inb(port) & ~(1 << irq));
}

void pic_send_eoi(uint8_t irq) {
    if (irq >= 8) outb(PIC2_CMD, 0x20);
    outb(PIC1_CMD, 0x20);
}