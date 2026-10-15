// desktop.c
#include <stdint.h>
#include "desktop.h"
#include "gfx.h"
#include "mouse.h"
#include "keyboard.h"
#include "timer.h"
#include "pmm.h"
#include "term.h"
#include "string.h"
#include "io.h"

#define COL_BG      0x00285A5A
#define COL_TASKBAR 0x00202030
#define COL_GRAY    0x00C0C0C0
#define COL_TITLE   0x00000080
#define COL_WHITE   0x00FFFFFF
#define COL_BLACK   0x00000000

#define TB_H   28
#define TB_Y   (GFX_H - TB_H)
#define WIN_X  262
#define WIN_Y  150
#define WIN_W  500
#define WIN_H  300
#define BTN_X  (WIN_X + 30)
#define BTN_Y  (WIN_Y + 150)
#define BTN_W  140
#define BTN_H  32

static uint32_t clicks = 0;

static void append_num(char *buf, uint32_t n) {
    char tmp[11];
    int i = 0;
    if (n == 0) tmp[i++] = '0';
    while (n) { tmp[i++] = '0' + (n % 10); n /= 10; }
    char *p = buf + strlen(buf);
    while (i) *p++ = tmp[--i];
    *p = 0;
}

static void append(char *buf, const char *s) { strcpy(buf + strlen(buf), s); }

static void pad(char *buf, int n) {
    int l = (int)strlen(buf);
    while (l < n) buf[l++] = ' ';
    buf[l] = 0;
}

static int in_rect(int px, int py, int x, int y, int w, int h) {
    return px >= x && px < x + w && py >= y && py < y + h;
}

static void draw_button(int pressed) {
    gfx_rect(BTN_X, BTN_Y, BTN_W, BTN_H, pressed ? 0x00A0A0A0 : 0x00E0E0E0);
    gfx_frame(BTN_X, BTN_Y, BTN_W, BTN_H, COL_BLACK);
    gfx_text(BTN_X + 34, BTN_Y + 8, "Click me!", COL_BLACK, GFX_NOBG);
}

static void draw_clicks(void) {
    char buf[32];
    strcpy(buf, "Clicks: ");
    append_num(buf, clicks);
    pad(buf, 14);
    gfx_text(WIN_X + 200, WIN_Y + 158, buf, COL_BLACK, COL_GRAY);
}

static void draw_clock(uint32_t secs) {
    char buf[32];
    strcpy(buf, "Uptime: ");
    append_num(buf, secs);
    append(buf, "s");
    pad(buf, 16);
    gfx_text(GFX_W - 16 * 8 - 10, TB_Y + 6, buf, COL_WHITE, COL_TASKBAR);
}

static void draw_scene(void) {
    gfx_fill(COL_BG);

    gfx_rect(0, TB_Y, GFX_W, TB_H, COL_TASKBAR);
    gfx_text(10, TB_Y + 6, "Danbo Desktop", COL_WHITE, COL_TASKBAR);

    gfx_rect(WIN_X + 5, WIN_Y + 5, WIN_W, WIN_H, 0x00101818);     // shadow
    gfx_rect(WIN_X, WIN_Y, WIN_W, WIN_H, COL_GRAY);
    gfx_rect(WIN_X, WIN_Y, WIN_W, 24, COL_TITLE);
    gfx_text(WIN_X + 8, WIN_Y + 4, "Welcome to Danbo Desktop", COL_WHITE, GFX_NOBG);
    gfx_frame(WIN_X, WIN_Y, WIN_W, WIN_H, COL_BLACK);

    char buf[48];
    gfx_text(WIN_X + 16, WIN_Y + 44, "Danbo kernel v0.1 (x86)", COL_BLACK, GFX_NOBG);
    strcpy(buf, "Free RAM: ");
    append_num(buf, pmm_free_pages() / 256);
    append(buf, " MB");
    gfx_text(WIN_X + 16, WIN_Y + 68, buf, COL_BLACK, GFX_NOBG);
    gfx_text(WIN_X + 16, WIN_Y + 104, "Move the mouse and click the button.", COL_BLACK, GFX_NOBG);
    gfx_text(WIN_X + 16, WIN_Y + WIN_H - 28, "Press ESC to return to the shell.", COL_BLACK, GFX_NOBG);
}

void desktop_run(void) {
    int rc = gfx_init();
    if (rc != 0) {
        term_print("Could not start graphics mode: ");
        if (rc == -1)      term_print("no QEMU/Bochs VGA card found (run QEMU with -vga std)\n");
        else if (rc == -2) term_print("VBE extensions not available\n");
        else               term_print("could not map the framebuffer\n");
        return;
    }

    clicks = 0;
    mouse_set_bounds(GFX_W, GFX_H);
    draw_scene();
    draw_button(0);
    draw_clicks();

    int mx, my;
    uint8_t mb;
    mouse_get(&mx, &my, &mb);
    gfx_cursor_move(mx, my);
    gfx_cursor_show();

    int lastx = mx, lasty = my, prev_left = 0, pressed = 0;
    uint32_t last_secs = 0xFFFFFFFFu;

    for (;;) {
        char c;
        if (keyboard_poll(&c) && c == 27) break;               // Esc exits

        uint32_t secs = timer_ticks() / 100;
        if (secs != last_secs) {
            last_secs = secs;
            gfx_cursor_hide();
            draw_clock(secs);
            gfx_cursor_show();
        }

        mouse_get(&mx, &my, &mb);
        if (mx != lastx || my != lasty) {
            gfx_cursor_move(mx, my);
            lastx = mx; lasty = my;
        }

        int left = mb & 1;
        if (left != prev_left) {
            int inside = in_rect(mx, my, BTN_X, BTN_Y, BTN_W, BTN_H);
            gfx_cursor_hide();
            if (left && inside) {
                pressed = 1;
                draw_button(1);
            } else if (!left && pressed) {
                pressed = 0;
                if (inside) clicks++;                          // counts on release, like a real button
                draw_button(0);
                draw_clicks();
            }
            gfx_cursor_show();
            prev_left = left;
        }

        __asm__ volatile ("hlt");                              // sleep until the next interrupt
    }

    outb(0x64, 0xFE);                       // reset the CPU; Danbo reboots straight to the shell
    for (;;) __asm__ volatile ("hlt");
}