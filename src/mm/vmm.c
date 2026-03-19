#include "vmm.h"
#include "pmm.h"
#include "../panic/panic.h"
#include "../cpu.h"

#define PAGE_MASK  (~0xFFFULL)

// Extract page-table indices from a virtual address
#define PML4_IDX(v) (((v) >> 39) & 0x1FF)
#define PDPT_IDX(v) (((v) >> 30) & 0x1FF)
#define PD_IDX(v)   (((v) >> 21) & 0x1FF)
#define PT_IDX(v)   (((v) >> 12) & 0x1FF)

static uint64_t* get_or_alloc_table(uint64_t* entry_ptr, uint64_t flags) {
    if (!(*entry_ptr & VMM_FLAG_PRESENT)) {
        void* page = pmm_alloc();
        if (!page) panic(PANIC_OUT_OF_MEMORY);
        uint64_t* p = (uint64_t*)page;
        for (int i = 0; i < 512; i++) p[i] = 0;
        *entry_ptr = (uint64_t)page | flags;
    }
    return (uint64_t*)(*entry_ptr & PAGE_MASK);
}

void vmm_init(void) {
    if (!read_cr3()) panic(PANIC_VMM_CR3_INVALID);
    // Boot identity map remains in place; nothing else needed
}

void vmm_map(uint64_t virt, uint64_t phys, uint64_t flags) {
    if (virt & 0xFFF) panic(PANIC_VMM_UNALIGNED_ADDRESS);
    if (phys & 0xFFF) panic(PANIC_VMM_UNALIGNED_ADDRESS);

    uint64_t* pml4 = (uint64_t*)(read_cr3() & PAGE_MASK);

    uint64_t* pdpt = get_or_alloc_table(&pml4[PML4_IDX(virt)], VMM_KERNEL_DATA);
    uint64_t* pd   = get_or_alloc_table(&pdpt[PDPT_IDX(virt)], VMM_KERNEL_DATA);

    uint64_t* pde = &pd[PD_IDX(virt)];
    if (*pde & VMM_FLAG_PS) panic(PANIC_VMM_HUGEPAGE_CONFLICT);

    uint64_t* pt = get_or_alloc_table(pde, VMM_KERNEL_DATA);
    pt[PT_IDX(virt)] = phys | flags;

    invlpg(virt);
}

void vmm_unmap(uint64_t virt) {
    if (virt & 0xFFF) panic(PANIC_VMM_UNALIGNED_ADDRESS);

    uint64_t* pml4 = (uint64_t*)(read_cr3() & PAGE_MASK);

    uint64_t pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & VMM_FLAG_PRESENT)) return;
    uint64_t* pdpt = (uint64_t*)(pml4e & PAGE_MASK);

    uint64_t pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & VMM_FLAG_PRESENT)) return;
    uint64_t* pd = (uint64_t*)(pdpte & PAGE_MASK);

    uint64_t pde = pd[PD_IDX(virt)];
    if (!(pde & VMM_FLAG_PRESENT)) return;
    uint64_t* pt = (uint64_t*)(pde & PAGE_MASK);

    pt[PT_IDX(virt)] = 0;
    invlpg(virt);
}

uint64_t vmm_get_phys(uint64_t virt) {
    uint64_t* pml4 = (uint64_t*)(read_cr3() & PAGE_MASK);

    uint64_t pml4e = pml4[PML4_IDX(virt)];
    if (!(pml4e & VMM_FLAG_PRESENT)) return 0;
    uint64_t* pdpt = (uint64_t*)(pml4e & PAGE_MASK);

    uint64_t pdpte = pdpt[PDPT_IDX(virt)];
    if (!(pdpte & VMM_FLAG_PRESENT)) return 0;
    uint64_t* pd = (uint64_t*)(pdpte & PAGE_MASK);

    uint64_t pde = pd[PD_IDX(virt)];
    if (!(pde & VMM_FLAG_PRESENT)) return 0;

    if (pde & VMM_FLAG_PS) {
        // 2MB huge page: base is bits [51:21], add 2MB-page offset
        return (pde & ~0x1FFFFFULL) | (virt & 0x1FFFFF);
    }

    uint64_t* pt = (uint64_t*)(pde & PAGE_MASK);
    uint64_t pte = pt[PT_IDX(virt)];
    if (!(pte & VMM_FLAG_PRESENT)) return 0;
    return pte & PAGE_MASK;
}
