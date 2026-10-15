// mouse.c
#include <stdint.h>
#include "mouse.h"
#include "irq.h"
#include "io.h"

static volatile int mx = 0, my = 0;
static volatile uint8_t mb = 0;
static int bound_w = 640, bound_h = 480;
static uint8_t cycle = 0;
static uint8_t pkt[3];

static void wait_write(void) { for (uint32_t t = 0; t < 100000; t++) if (!(inb(0x64) & 2)) return; }
static void wait_read(void)  { for (uint32_t t = 0; t < 100000; t++) if (inb(0x64) & 1) return; }

static void mouse_write(uint8_t v) {
    wait_write(); outb(0x64, 0xD4);     // next byte goes to the mouse, not the keyboard
    wait_write(); outb(0x60, v);
    wait_read();  inb(0x60);            // swallow the ACK (0xFA)
}

static void mouse_callback(struct registers *r) {
    (void)r;
    uint8_t d = inb(0x60);

    if (cycle == 0 && !(d & 0x08)) return;       // bit 3 is always set in byte 0: resync
    pkt[cycle++] = d;
    if (cycle < 3) return;
    cycle = 0;

    if (pkt[0] & 0xC0) return;                   // overflow, discard packet

    // dx/dy are 9-bit signed: sign bits live in the first byte
    int dx = (int)pkt[1] - (((int)pkt[0] << 4) & 0x100);
    int dy = (int)pkt[2] - (((int)pkt[0] << 3) & 0x100);

    int nx = mx + dx;
    int ny = my - dy;                            // mouse Y is up, screen Y is down
    if (nx < 0) nx = 0;
    if (ny < 0) ny = 0;
    if (nx >= bound_w) nx = bound_w - 1;
    if (ny >= bound_h) ny = bound_h - 1;

    mx = nx; my = ny;
    mb = pkt[0] & 7;
}

void mouse_init(void) {
    wait_write(); outb(0x64, 0xA8);              // enable the second PS/2 port
    wait_write(); outb(0x64, 0x20);              // read controller config
    wait_read();
    uint8_t cfg = inb(0x60);
    cfg |= 0x02;                                 // enable IRQ12
    cfg &= ~0x20;                                // enable mouse clock
    wait_write(); outb(0x64, 0x60);
    wait_write(); outb(0x60, cfg);

    mouse_write(0xF6);                           // defaults
    mouse_write(0xF4);                           // start streaming packets

    irq_install_handler(12, mouse_callback);
}

void mouse_set_bounds(int w, int h) {
    bound_w = w; bound_h = h;
    mx = w / 2; my = h / 2;
}

void mouse_get(int *x, int *y, uint8_t *buttons) {
    __asm__ volatile ("cli");
    *x = mx; *y = my; *buttons = mb;
    __asm__ volatile ("sti");
}