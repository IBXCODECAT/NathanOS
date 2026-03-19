#include "proc.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../gdt/gdt.h"
#include "../syscall/syscall.h"
#include "../elf/elf.h"

/* User stack lives just above the 64MB identity map, one page */
#define USER_STACK_BASE 0x4010000ULL

__attribute__((noreturn)) void process_run_elf(const void *elf_data) {
    /* 1. Create a private address space for this process */
    uint64_t as = vmm_create_address_space();

    /* 2. Allocate a dedicated kernel stack.
          TSS.rsp0 is used when an interrupt fires in ring 3.
          cpu_local.kernel_rsp is used on SYSCALL entry.
          Both must point into this stack so the kernel never touches
          the user stack during privilege transitions. */
    uint8_t *kstack     = (uint8_t *)pmm_alloc();
    uint64_t kstack_top = (uint64_t)(kstack + 4096);
    gdt_set_kernel_stack(kstack_top);
    syscall_set_kernel_stack(kstack_top);

    /* 3. Switch into the new address space and load the ELF.
          vmm_map() calls inside elf_load() now populate the per-process
          page tables rather than the kernel's. */
    vmm_switch(as);
    uint64_t entry = elf_load(elf_data);

    /* 4. Map one page for the user stack in the same address space */
    uint8_t *ustack = (uint8_t *)pmm_alloc();
    vmm_map(USER_STACK_BASE, (uint64_t)ustack, VMM_USER_DATA);

    /* 5. Drop into ring 3 — CR3 is already set to `as` */
    ring3_enter(entry, USER_STACK_BASE + 0x1000);
}
