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

#endif
