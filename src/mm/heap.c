#include "heap.h"
#include "pmm.h"
#include "../panic/panic.h"

#include <stdint.h>
#include <stddef.h>

#define PAGE_SIZE       4096
#define HEAP_INIT_PAGES 16      // 64 KB initial heap
#define HEAP_GROW_PAGES 4       // 16 KB added per expansion
#define ALIGN           16      // every allocation is 16-byte aligned

// Each allocation is preceded by this header in memory.
// Layout: [ block_t header | payload bytes ... ]
typedef struct block {
    size_t        size;   // payload bytes (does not include sizeof(block_t))
    int           free;   // 1 = available, 0 = in use
    struct block* next;   // next block in address order
} block_t;

static block_t* heap_head = NULL;

// ─── Internal helpers ─────────────────────────────────────────────────────────

static size_t align_up(size_t n) {
    return (n + ALIGN - 1) & ~((size_t)(ALIGN - 1));
}

// Allocate `pages` sequential pages from the PMM and return a pointer to the
// first one.  Sequential pmm_alloc() calls return adjacent pages because the
// PMM scans the bitmap linearly and we are single-threaded.
static void* alloc_pages(size_t pages) {
    void* base = pmm_alloc();
    if (!base) panic(PANIC_OUT_OF_MEMORY);
    for (size_t i = 1; i < pages; i++) {
        if (!pmm_alloc()) panic(PANIC_OUT_OF_MEMORY);
    }
    return base;
}

// Merge all adjacent free blocks into one (single forward pass).
static void coalesce(void) {
    block_t* b = heap_head;
    while (b && b->next) {
        if (b->free && b->next->free) {
            b->size += sizeof(block_t) + b->next->size;
            b->next  = b->next->next;
        } else {
            b = b->next;
        }
    }
}

// Grow the heap by HEAP_GROW_PAGES pages.  If the new region is physically
// adjacent to the last block and that block is free, extend it in place to
// avoid unnecessary fragmentation.
static void heap_expand(void) {
    void*   mem         = alloc_pages(HEAP_GROW_PAGES);
    size_t  added_bytes = HEAP_GROW_PAGES * PAGE_SIZE;

    block_t* last = heap_head;
    while (last->next) last = last->next;

    uint8_t* last_end = (uint8_t*)last + sizeof(block_t) + last->size;

    if (last->free && last_end == (uint8_t*)mem) {
        // Adjacent free block — just extend it
        last->size += added_bytes;
    } else {
        // Append a new free block
        block_t* blk = (block_t*)mem;
        blk->size    = added_bytes - sizeof(block_t);
        blk->free    = 1;
        blk->next    = NULL;
        last->next   = blk;
    }
}

// ─── Public API ───────────────────────────────────────────────────────────────

void heap_init(void) {
    void* mem    = alloc_pages(HEAP_INIT_PAGES);
    heap_head        = (block_t*)mem;
    heap_head->size  = HEAP_INIT_PAGES * PAGE_SIZE - sizeof(block_t);
    heap_head->free  = 1;
    heap_head->next  = NULL;
}

void* kmalloc(size_t size) {
    if (!size) return NULL;
    size = align_up(size);

    for (;;) {
        block_t* b = heap_head;
        while (b) {
            if (b->free && b->size >= size) {
                // Split only if the leftover is large enough to be useful
                if (b->size >= size + sizeof(block_t) + ALIGN) {
                    block_t* split = (block_t*)((uint8_t*)b + sizeof(block_t) + size);
                    split->size    = b->size - size - sizeof(block_t);
                    split->free    = 1;
                    split->next    = b->next;
                    b->next        = split;
                    b->size        = size;
                }
                b->free = 0;
                return (void*)((uint8_t*)b + sizeof(block_t));
            }
            b = b->next;
        }
        // No suitable block — grow and retry
        heap_expand();
    }
}

void kfree(void* ptr) {
    if (!ptr) return;
    block_t* hdr = (block_t*)((uint8_t*)ptr - sizeof(block_t));
    if (hdr->free) panic(PANIC_HEAP_DOUBLE_FREE);
    hdr->free = 1;
    coalesce();
}

size_t heap_used_bytes(void) {
    size_t used = 0;
    for (block_t* b = heap_head; b; b = b->next)
        if (!b->free) used += sizeof(block_t) + b->size;
    return used;
}

size_t heap_free_bytes(void) {
    size_t free = 0;
    for (block_t* b = heap_head; b; b = b->next)
        if (b->free) free += b->size;
    return free;
}
