#ifndef PMM_H
#define PMM_H

#include <stdint.h>
#include "multiboot.h"

#define PAGE_SIZE 4096

void     pmm_init(struct multiboot_info *mbi);
uint32_t pmm_alloc_page(void);          // returns physical address, 0 on failure
void     pmm_free_page(uint32_t addr);
uint32_t pmm_total_pages(void);
uint32_t pmm_free_pages(void);

uint32_t pmm_highest_page(void);
#endif