#ifndef VMM_H
#define VMM_H
#include <stdint.h>

// Page table entry flag bits
#define VMM_FLAG_PRESENT   (1ULL << 0)
#define VMM_FLAG_WRITABLE  (1ULL << 1)
#define VMM_FLAG_USER      (1ULL << 2)
#define VMM_FLAG_PS        (1ULL << 7)   // huge page (set by boot.asm)
#define VMM_FLAG_NX        (1ULL << 63)

// Convenience combinations
#define VMM_KERNEL_DATA  (VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE)
#define VMM_KERNEL_CODE  (VMM_FLAG_PRESENT)
#define VMM_USER_DATA    (VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE | VMM_FLAG_USER)
#define VMM_USER_CODE    (VMM_FLAG_PRESENT | VMM_FLAG_USER)

void     vmm_init(void);
void     vmm_map(uint64_t virt, uint64_t phys, uint64_t flags);
void     vmm_unmap(uint64_t virt);
uint64_t vmm_get_phys(uint64_t virt);   // returns 0 if not mapped

/*
 * Address-space management.
 *
 * vmm_create_address_space() — allocate a fresh PML4 that contains a
 *   private copy of the kernel's intermediate page tables.  The identity
 *   map huge-page entries are duplicated so the kernel remains reachable
 *   from inside the new space; all non-huge (user) mappings start empty.
 *   Returns the physical address to load into CR3.
 *
 * vmm_switch()       — write a CR3 value (full TLB flush).
 * vmm_kernel_cr3()   — physical address of the kernel's own PML4.
 */
uint64_t vmm_create_address_space(void);
void     vmm_switch(uint64_t cr3);
uint64_t vmm_kernel_cr3(void);

#endif
