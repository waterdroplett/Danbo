// gfx.h
#ifndef GFX_HEADER_H
#define GFX_HEADER_H

#include <stdint.h>

#define GFX_W 1024
#define GFX_H 768
#define GFX_NOBG 0xFF000000u        // pass as bg to draw text with a transparent background

int  gfx_init(void);                // 0 = ok, -1 = no VGA card found, -2 = no VBE, -3 = mapping failed
void gfx_shutdown(void);            // back to text mode
void gfx_fill(uint32_t color);
void gfx_rect(int x, int y, int w, int h, uint32_t color);
void gfx_frame(int x, int y, int w, int h, uint32_t color);
void gfx_text(int x, int y, const char *s, uint32_t fg, uint32_t bg);
void gfx_cursor_move(int x, int y);
void gfx_cursor_show(void);
void gfx_cursor_hide(void);

#endif