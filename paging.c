#include "paging.h"
#include "pmm.h"

static uint32_t *page_dir;

static void zero_page(void *p) {
    uint32_t *w = (uint32_t *)p;
    for (int i = 0; i < 1024; i++) w[i] = 0;
}

int paging_map(uint32_t virt, uint32_t phys, uint32_t flags) {
    uint32_t pdi = virt >> 22;               // which page table
    uint32_t pti = (virt >> 12) & 0x3FF;     // which entry in that table

    if (!(page_dir[pdi] & PAGE_PRESENT)) {   // create the page table on demand
        uint32_t t = pmm_alloc_page();
        if (!t) return -1;
        zero_page((void *)t);
        page_dir[pdi] = t | PAGE_PRESENT | PAGE_WRITE | (flags & PAGE_USER);
    }

    uint32_t *table = (uint32_t *)(page_dir[pdi] & 0xFFFFF000);
    table[pti] = (phys & 0xFFFFF000) | (flags & 0xFFF) | PAGE_PRESENT;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
    return 0;
}

void paging_unmap(uint32_t virt) {
    uint32_t pdi = virt >> 22;
    uint32_t pti = (virt >> 12) & 0x3FF;
    if (!(page_dir[pdi] & PAGE_PRESENT)) return;

    uint32_t *table = (uint32_t *)(page_dir[pdi] & 0xFFFFF000);
    table[pti] = 0;
    __asm__ volatile ("invlpg (%0)" : : "r"(virt) : "memory");
}

void paging_init(void) {
    page_dir = (uint32_t *)pmm_alloc_page();
    zero_page(page_dir);

    // Identity-map all RAM, except page 0 so NULL pointer accesses fault
    uint32_t pages = pmm_highest_page();
    for (uint32_t p = 1; p < pages; p++)
        paging_map(p << 12, p << 12, PAGE_WRITE);

    uint32_t cr0;
    __asm__ volatile ("mov %0, %%cr3" : : "r"((uint32_t)page_dir));
    __asm__ volatile ("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000;                       // PG bit
    __asm__ volatile ("mov %0, %%cr0" : : "r"(cr0));
}