#include "pmm.h"
#include "../panic/panic.h"
#include <stddef.h>

#define PAGE_SIZE 4096

extern uint64_t _kernel_end;   // symbol from linker.ld

// Bitmap is placed at _kernel_end at runtime — one bit per 4KB page.
// Size = total_pages / 8 bytes, so it scales with however much RAM GRUB reports.
// Requires the identity map to cover kernel + bitmap (boot.asm maps 64MB,
// which is enough for bitmaps tracking up to ~500 GB of RAM).
static uint8_t*  bitmap            = NULL;
static uint64_t  total_pages_count = 0;
static uint64_t  free_pages_count  = 0;

static void bitmap_set(uint64_t bit) {
    bitmap[bit / 8] |= (uint8_t)(1 << (bit % 8));
}

static void bitmap_clear(uint64_t bit) {
    bitmap[bit / 8] &= (uint8_t)~(1 << (bit % 8));
}

static int bitmap_test(uint64_t bit) {
    return bitmap[bit / 8] & (1 << (bit % 8));
}

#define IDENTITY_MAP_LIMIT (64ULL * 1024 * 1024)  // 64 MB mapped at boot

void pmm_init(multiboot_info_t* mbi) {
    if (!mbi) panic(PANIC_PMM_NULL_MULTIBOOT_INFO);

    // Multiboot flags bit 0: mem_lower/mem_upper fields are valid
    if (!(mbi->flags & (1 << 0))) panic(PANIC_PMM_MULTIBOOT_MEM_UNAVAILABLE);

    uint64_t total_kb = (uint64_t)mbi->mem_lower + (uint64_t)mbi->mem_upper;
    total_pages_count = (total_kb * 1024) / PAGE_SIZE;

    if (total_pages_count == 0) panic(PANIC_PMM_NO_MEMORY_DETECTED);

    // Place bitmap right after kernel; size scales with total RAM
    bitmap = (uint8_t*)((uint64_t)&_kernel_end);
    uint64_t bitmap_bytes = (total_pages_count + 7) / 8;

    // First free page is after kernel + bitmap
    uint64_t bitmap_end  = (uint64_t)&_kernel_end + bitmap_bytes;
    uint64_t first_free  = (bitmap_end + PAGE_SIZE - 1) / PAGE_SIZE;

    // Bitmap must fit within the identity-mapped window or we can't initialise it
    if (bitmap_end > IDENTITY_MAP_LIMIT) panic(PANIC_PMM_BITMAP_EXCEEDS_MAPPED_REGION);

    // Mark everything used
    for (uint64_t i = 0; i < bitmap_bytes; i++)
        bitmap[i] = 0xFF;

    free_pages_count = 0;
    for (uint64_t i = first_free; i < total_pages_count; i++) {
        bitmap_clear(i);
        free_pages_count++;
    }

    // Keep low 1 MB (pages 0–255) marked as used (BIOS/VGA/ROM)
    for (uint64_t i = 0; i < 256; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages_count--;
        }
    }
}

void* pmm_alloc(void) {
    for (uint64_t i = 0; i < total_pages_count; i++) {
        if (!bitmap_test(i)) {
            bitmap_set(i);
            free_pages_count--;
            return (void*)(i * PAGE_SIZE);
        }
    }
    return NULL;
}

void pmm_free(void* page) {
    uint64_t idx = (uint64_t)page / PAGE_SIZE;
    if (idx >= total_pages_count) panic(PANIC_PMM_FREE_OUT_OF_RANGE);
    if (!bitmap_test(idx))        panic(PANIC_PMM_DOUBLE_FREE);
    bitmap_clear(idx);
    free_pages_count++;
}

uint64_t pmm_free_pages(void) {
    return free_pages_count;
}

uint64_t pmm_total_pages(void) {
    return total_pages_count;
}
