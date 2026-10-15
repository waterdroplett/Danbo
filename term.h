#ifndef TERM_H
#define TERM_H

#include <stdint.h>

void term_clear(void);
void term_putc(char c);
void term_print(const char *s);
void term_set_color(uint8_t fg, uint8_t bg);
void term_print_hex(uint32_t n);
void term_print_dec(uint32_t n);
void term_print_int(int32_t n);

#endif