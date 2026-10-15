// mouse.h
#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

void mouse_init(void);
void mouse_set_bounds(int w, int h);                    // also recenters the pointer
void mouse_get(int *x, int *y, uint8_t *buttons);       // bit0 left, bit1 right, bit2 middle

#endif