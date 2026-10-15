#ifndef PAGING_H
#define PAGING_H

#include <stdint.h>

#define PAGE_PRESENT 0x1
#define PAGE_WRITE   0x2
#define PAGE_USER    0x4

void paging_init(void);
int  paging_map(uint32_t virt, uint32_t phys, uint32_t flags);   // 0 = ok, -1 = out of memory
void paging_unmap(uint32_t virt);

#endif