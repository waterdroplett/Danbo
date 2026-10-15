#include "pmm.h"
#include "cpu.h"

#define MAX_PAGES (1024 * 1024)          // enough to track 4 GB

static uint32_t bitmap[MAX_PAGES / 32];  // bit = 1 means used
static uint32_t total_pages = 0;
static uint32_t free_pages  = 0;
static uint32_t search_hint = 0;
static uint32_t highest_page = 0;

extern uint8_t kernel_end[];

static void mark_free(uint32_t p) {
    if (bitmap[p >> 5] & (1u << (p & 31))) {
        bitmap[p >> 5] &= ~(1u << (p & 31));
        free_pages++;
    }
}

static void mark_used(uint32_t p) {
    if (!(bitmap[p >> 5] & (1u << (p & 31)))) {
        bitmap[p >> 5] |= (1u << (p & 31));
        free_pages--;
    }
}

static void free_region(uint64_t base, uint64_t len) {
    if (base >> 32) return;                       // above 4 GB, ignore
    uint64_t end = base + len;
    if (end > 0x100000000ULL) end = 0x100000000ULL;

    uint32_t first = (uint32_t)((base + PAGE_SIZE - 1) >> 12);  // round up
    uint32_t last  = (uint32_t)(end >> 12);  
    if (last > highest_page) highest_page = last;                   // round down
    for (uint32_t p = first; p < last; p++) {
        mark_free(p);
        total_pages++;
    }
}

static void reserve_range(uint32_t start, uint32_t end) {
    for (uint32_t p = start >> 12; p < ((end + PAGE_SIZE - 1) >> 12); p++)
        mark_used(p);
}

void pmm_init(struct multiboot_info *mbi) {
    for (uint32_t i = 0; i < MAX_PAGES / 32; i++)
        bitmap[i] = 0xFFFFFFFF;                   // start with everything "used"

    if (mbi->flags & (1 << 6)) {                  // full memory map available
        uint32_t addr = mbi->mmap_addr;
        uint32_t end  = addr + mbi->mmap_length;
        while (addr < end) {
            struct mmap_entry *e = (struct mmap_entry *)addr;
            if (e->type == 1)
                free_region(e->addr, e->len);
            addr += e->size + 4;
        }
    } else if (mbi->flags & 1) {                  // fallback: just mem_upper
        free_region(0x100000, (uint64_t)mbi->mem_upper * 1024);
    }

    // Reserve what's already in use
    reserve_range(0, 0x100000);                              // first 1 MB (BIOS, VGA, etc.)
    reserve_range(0x100000, (uint32_t)kernel_end);           // the kernel itself
    reserve_range((uint32_t)mbi, (uint32_t)mbi + sizeof(*mbi));
    if (mbi->flags & (1 << 6))
        reserve_range(mbi->mmap_addr, mbi->mmap_addr + mbi->mmap_length);
}

static uint32_t alloc_page_unlocked(void) {
    for (uint32_t n = 0; n < MAX_PAGES / 32; n++) {
        uint32_t i = (search_hint + n) % (MAX_PAGES / 32);
        if (bitmap[i] == 0xFFFFFFFF) continue;
        for (int b = 0; b < 32; b++) {
            if (!(bitmap[i] & (1u << b))) {
                bitmap[i] |= (1u << b);
                free_pages--;
                search_hint = i;
                return (i * 32 + b) * PAGE_SIZE;
            }
        }
    }
    return 0;                                     // out of memory (page 0 is never free)
}

static void free_page_unlocked(uint32_t addr) {
    uint32_t p = addr >> 12;
    if (p >= MAX_PAGES) return;
    mark_free(p);
    if ((p >> 5) < search_hint) search_hint = p >> 5;
}

uint32_t pmm_total_pages(void) { return total_pages; }
uint32_t pmm_free_pages(void)  { return free_pages; }
uint32_t pmm_highest_page(void) { return highest_page; }

uint32_t pmm_alloc_page(void) {
    uint32_t flags = irq_save();
    uint32_t p = alloc_page_unlocked();
    irq_restore(flags);
    return p;
}

void pmm_free_page(uint32_t addr) {
    uint32_t flags = irq_save();
    free_page_unlocked(addr);
    irq_restore(flags);
}