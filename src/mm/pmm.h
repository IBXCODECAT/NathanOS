#ifndef PMM_H
#define PMM_H
#include <stdint.h>

// Multiboot v1 info struct (flags bit 1 gives mem_lower/mem_upper)
typedef struct {
    uint32_t flags;
    uint32_t mem_lower;   // KB below 1MB
    uint32_t mem_upper;   // KB above 1MB
} __attribute__((packed)) multiboot_info_t;

void  pmm_init(multiboot_info_t* mbi);

void* pmm_alloc(void);          // returns physical page address, NULL if OOM
void  pmm_free(void* page);

uint64_t pmm_free_pages(void);
uint64_t pmm_total_pages(void);

#endif
