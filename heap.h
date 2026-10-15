#ifndef HEAP_H
#define HEAP_H

#include <stdint.h>

void  heap_init(void);
void *kmalloc(uint32_t size);
void *kcalloc(uint32_t n, uint32_t size);   // zeroed memory
void  kfree(void *ptr);
void  heap_dump(void);                      // debug: print every block

#endif