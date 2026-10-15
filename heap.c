#include "heap.h"
#include "pmm.h"
#include "paging.h"
#include "string.h"
#include "term.h"
#include "cpu.h"

#define HEAP_START      0xD0000000u      // virtual address range for the heap
#define HEAP_LIMIT      0xE0000000u      // 256 MB of virtual space
#define HEAP_INIT_PAGES 4                // start with 16 KB
#define BLOCK_MAGIC     0xB10CB10Cu

struct block {
    uint32_t magic;
    uint32_t size;          // payload size in bytes
    uint32_t free;
    uint32_t _pad;          // keeps the header at 24 bytes so payloads stay 8-aligned
    struct block *next;
    struct block *prev;
};

static struct block *head = 0;
static struct block *tail = 0;
static uint32_t heap_end = HEAP_START;

// Map more pages at the end of the heap. Returns bytes added (0 on failure).
static uint32_t heap_grow(uint32_t bytes) {
    uint32_t pages = (bytes + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t added = 0;

    while (pages--) {
        if (heap_end >= HEAP_LIMIT) break;
        uint32_t phys = pmm_alloc_page();
        if (!phys) break;
        if (paging_map(heap_end, phys, PAGE_WRITE) != 0) {
            pmm_free_page(phys);
            break;
        }
        heap_end += PAGE_SIZE;
        added += PAGE_SIZE;
    }
    if (!added) return 0;

    if (tail && tail->free) {
        tail->size += added;                         // extend the last free block
    } else {
        struct block *b = (struct block *)(heap_end - added);
        b->magic = BLOCK_MAGIC;
        b->size  = added - sizeof(struct block);
        b->free  = 1;
        b->next  = 0;
        b->prev  = tail;
        if (tail) tail->next = b; else head = b;
        tail = b;
    }
    return added;
}

void heap_init(void) {
    heap_grow(HEAP_INIT_PAGES * PAGE_SIZE);
}

static void *kmalloc_unlocked(uint32_t size) {
    if (!size) return 0;
    size = (size + 7) & ~7u;                         // 8-byte alignment

    for (;;) {
        for (struct block *b = head; b; b = b->next) {
            if (!b->free || b->size < size) continue;

            // Split if the leftover is big enough to be its own block
            if (b->size >= size + sizeof(struct block) + 8) {
                struct block *n = (struct block *)((uint8_t *)(b + 1) + size);
                n->magic = BLOCK_MAGIC;
                n->size  = b->size - size - sizeof(struct block);
                n->free  = 1;
                n->next  = b->next;
                n->prev  = b;
                if (n->next) n->next->prev = n; else tail = n;
                b->next = n;
                b->size = size;
            }
            b->free = 0;
            return b + 1;
        }
        // Nothing fits: grow the heap and search again
        if (!heap_grow(size + sizeof(struct block))) return 0;
    }
}

void *kcalloc(uint32_t n, uint32_t size) {
    if (n && size > 0xFFFFFFFFu / n) return 0;       // overflow check
    void *p = kmalloc(n * size);
    if (p) memset(p, 0, n * size);
    return p;
}

static void kfree_unlocked(void *ptr) {
    if (!ptr) return;
    struct block *b = (struct block *)ptr - 1;

    if (b->magic != BLOCK_MAGIC) {
        term_print("kfree: bad pointer "); term_print_hex((uint32_t)ptr); term_putc('\n');
        return;
    }
    if (b->free) {
        term_print("kfree: double free "); term_print_hex((uint32_t)ptr); term_putc('\n');
        return;
    }
    b->free = 1;

    // Merge with the next block if it's free
    if (b->next && b->next->free) {
        struct block *n = b->next;
        b->size += sizeof(struct block) + n->size;
        b->next = n->next;
        if (n->next) n->next->prev = b; else tail = b;
        n->magic = 0;
    }
    // Merge into the previous block if it's free
    if (b->prev && b->prev->free) {
        struct block *p = b->prev;
        p->size += sizeof(struct block) + b->size;
        p->next = b->next;
        if (b->next) b->next->prev = p; else tail = p;
        b->magic = 0;
    }
}

void heap_dump(void) {
    for (struct block *b = head; b; b = b->next) {
        term_print("  ["); term_print_hex((uint32_t)b); term_print("] ");
        term_print_dec(b->size);
        term_print(b->free ? " bytes free\n" : " bytes used\n");
    }
}

void *kmalloc(uint32_t size) {
    uint32_t flags = irq_save();
    void *p = kmalloc_unlocked(size);
    irq_restore(flags);
    return p;
}

void kfree(void *ptr) {
    uint32_t flags = irq_save();
    kfree_unlocked(ptr);
    irq_restore(flags);
}