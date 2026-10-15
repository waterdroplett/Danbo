// gfx.c
#include <stdint.h>
#include "gfx.h"
#include "io.h"
#include "pci.h"
#include "pmm.h"
#include "paging.h"

#define VBE_INDEX_PORT 0x01CE
#define VBE_DATA_PORT  0x01CF
#define VBE_ID     0
#define VBE_XRES   1
#define VBE_YRES   2
#define VBE_BPP    3
#define VBE_ENABLE 4

static volatile uint32_t *fb = 0;
static uint8_t font[256 * 16];          // 8x16 glyphs, copied from the VGA text font

static void vbe_write(uint16_t idx, uint16_t val) {
    outw(VBE_INDEX_PORT, idx);
    outw(VBE_DATA_PORT, val);
}
static uint16_t vbe_read(uint16_t idx) {
    outw(VBE_INDEX_PORT, idx);
    return inw(VBE_DATA_PORT);
}

static uint8_t rd(uint16_t port, uint8_t i) { outb(port, i); return inb(port + 1); }
static void    wr(uint16_t port, uint8_t i, uint8_t v) { outb(port, i); outb(port + 1, v); }

// The BIOS font lives in VGA plane 2. Temporarily reconfigure the VGA so plane 2
// shows up at 0xA0000, copy it out, then put the registers back.
static uint8_t saved_s2, saved_s4, saved_g4, saved_g5, saved_g6;

static void vga_save_regs(void) {
    saved_s2 = rd(0x3C4, 2); saved_s4 = rd(0x3C4, 4);
    saved_g4 = rd(0x3CE, 4); saved_g5 = rd(0x3CE, 5); saved_g6 = rd(0x3CE, 6);
}

static void vga_restore_regs(void) {
    wr(0x3C4, 2, saved_s2); wr(0x3C4, 4, saved_s4);
    wr(0x3CE, 4, saved_g4); wr(0x3CE, 5, saved_g5); wr(0x3CE, 6, saved_g6);
}

// Make plane 2 (the font) visible at 0xA0000 so it can be read or written
static void vga_font_access(void) {
    wr(0x3C4, 2, 0x04);     // write only to plane 2
    wr(0x3C4, 4, 0x07);     // sequential memory access
    wr(0x3CE, 1, 0x00);     // no set/reset
    wr(0x3CE, 4, 0x02);     // read plane 2
    wr(0x3CE, 5, 0x00);     // write mode 0, no odd/even
    wr(0x3CE, 6, 0x00);     // map video memory at 0xA0000
    wr(0x3CE, 8, 0xFF);     // all bits writable
}

static void load_vga_font(void) {
    vga_save_regs();        // remember the text-mode register state
    vga_font_access();
    volatile uint8_t *src = (volatile uint8_t *)0xA0000;
    for (int c = 0; c < 256; c++)
        for (int r = 0; r < 16; r++)
            font[c * 16 + r] = src[c * 32 + r];
    vga_restore_regs();
}

static void store_vga_font(void) {
    vga_font_access();
    volatile uint8_t *dst = (volatile uint8_t *)0xA0000;
    for (int c = 0; c < 256; c++)
        for (int r = 0; r < 32; r++)
            dst[c * 32 + r] = (r < 16) ? font[c * 16 + r] : 0;
    vga_restore_regs();
}

int gfx_init(void) {
    load_vga_font();                            // must happen while still in text mode

    uint8_t bus, dev, fn;
    if (!pci_find(0x1234, 0x1111, &bus, &dev, &fn)) return -1;   // QEMU/Bochs VGA
    uint32_t bar0 = pci_read32(bus, dev, fn, 0x10) & 0xFFFFFFF0u;
    if (!bar0) return -1;

    uint16_t id = vbe_read(VBE_ID);
    if (id < 0xB0C0 || id > 0xB0CF) return -2;

    // The framebuffer sits far above RAM, so map it into our page tables
    uint32_t bytes = GFX_W * GFX_H * 4;
    for (uint32_t off = 0; off < bytes; off += PAGE_SIZE)
        if (paging_map(bar0 + off, bar0 + off, PAGE_WRITE) != 0) return -3;
    fb = (volatile uint32_t *)bar0;

    vbe_write(VBE_ENABLE, 0);
    vbe_write(VBE_XRES, GFX_W);
    vbe_write(VBE_YRES, GFX_H);
    vbe_write(VBE_BPP, 32);
    vbe_write(VBE_ENABLE, 0x41);                // enabled + linear framebuffer
    return 0;
}

void gfx_shutdown(void) {
    vbe_write(VBE_ENABLE, 0);   // leave VBE mode
    store_vga_font();           // the framebuffer trashed the font, so write it back
    inb(0x3DA);                 // reset the attribute controller flip-flop
    outb(0x3C0, 0x20);          // re-enable video output
}

void gfx_fill(uint32_t color) {
    for (uint32_t i = 0; i < GFX_W * GFX_H; i++) fb[i] = color;
}

void gfx_rect(int x, int y, int w, int h, uint32_t color) {
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > GFX_W) w = GFX_W - x;
    if (y + h > GFX_H) h = GFX_H - y;
    if (w <= 0 || h <= 0) return;
    for (int j = 0; j < h; j++) {
        volatile uint32_t *row = fb + (y + j) * GFX_W + x;
        for (int i = 0; i < w; i++) row[i] = color;
    }
}

void gfx_frame(int x, int y, int w, int h, uint32_t color) {
    gfx_rect(x, y, w, 1, color);
    gfx_rect(x, y + h - 1, w, 1, color);
    gfx_rect(x, y, 1, h, color);
    gfx_rect(x + w - 1, y, 1, h, color);
}

static void gfx_char(int x, int y, char ch, uint32_t fg, uint32_t bg) {
    const uint8_t *g = &font[(uint8_t)ch * 16];
    for (int r = 0; r < 16; r++) {
        for (int c = 0; c < 8; c++) {
            int px = x + c, py = y + r;
            if (px < 0 || px >= GFX_W || py < 0 || py >= GFX_H) continue;
            if (g[r] & (0x80 >> c)) fb[py * GFX_W + px] = fg;
            else if (!(bg & GFX_NOBG)) fb[py * GFX_W + px] = bg;
        }
    }
}

void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg) {
    for (; *s; s++, x += 8) gfx_char(x, y, *s, fg, bg);
}

// ---- Software mouse cursor (saves the pixels underneath so it can be erased) ----
#define CUR_W 12
#define CUR_H 17
static const char *cursor_art[CUR_H] = {
    "X           ",
    "XX          ",
    "X.X         ",
    "X..X        ",
    "X...X       ",
    "X....X      ",
    "X.....X     ",
    "X......X    ",
    "X.......X   ",
    "X........X  ",
    "X.....XXXXX ",
    "X..X..X     ",
    "X.X X..X    ",
    "XX  X..X    ",
    "X    X..X   ",
    "     X..X   ",
    "      XX    ",
};

static int cur_x = 0, cur_y = 0, cur_visible = 0;
static uint32_t under[CUR_W * CUR_H];

void gfx_cursor_show(void) {
    if (cur_visible) return;
    for (int j = 0; j < CUR_H; j++) {
        for (int i = 0; i < CUR_W; i++) {
            int px = cur_x + i, py = cur_y + j;
            if (px < 0 || px >= GFX_W || py < 0 || py >= GFX_H) continue;
            under[j * CUR_W + i] = fb[py * GFX_W + px];
            char c = cursor_art[j][i];
            if (c == 'X') fb[py * GFX_W + px] = 0x00000000;
            else if (c == '.') fb[py * GFX_W + px] = 0x00FFFFFF;
        }
    }
    cur_visible = 1;
}

void gfx_cursor_hide(void) {
    if (!cur_visible) return;
    for (int j = 0; j < CUR_H; j++) {
        for (int i = 0; i < CUR_W; i++) {
            int px = cur_x + i, py = cur_y + j;
            if (px < 0 || px >= GFX_W || py < 0 || py >= GFX_H) continue;
            char c = cursor_art[j][i];
            if (c == 'X' || c == '.') fb[py * GFX_W + px] = under[j * CUR_W + i];
        }
    }
    cur_visible = 0;
}

void gfx_cursor_move(int x, int y) {
    int was_visible = cur_visible;
    gfx_cursor_hide();
    cur_x = x; cur_y = y;
    if (was_visible) gfx_cursor_show();
}