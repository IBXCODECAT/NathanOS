#include "elf.h"
#include "../mm/vmm.h"
#include "../mm/pmm.h"
#include "../panic/panic.h"
#include <stdint.h>

uint64_t elf_load(const void *data) {
    const Elf64_Ehdr *ehdr = (const Elf64_Ehdr *)data;

    /* Validate header */
    if (ehdr->e_ident[EI_MAG0] != ELFMAG0 ||
        ehdr->e_ident[EI_MAG1] != ELFMAG1 ||
        ehdr->e_ident[EI_MAG2] != ELFMAG2 ||
        ehdr->e_ident[EI_MAG3] != ELFMAG3 ||
        ehdr->e_ident[EI_CLASS] != ELFCLASS64 ||
        ehdr->e_type            != ET_EXEC   ||
        ehdr->e_machine         != EM_X86_64)
        panic(PANIC_ELF_INVALID);

    const uint8_t *base = (const uint8_t *)data;

    for (uint16_t i = 0; i < ehdr->e_phnum; i++) {
        const Elf64_Phdr *phdr = (const Elf64_Phdr *)
            (base + ehdr->e_phoff + (uint64_t)i * ehdr->e_phentsize);

        if (phdr->p_type != PT_LOAD || phdr->p_memsz == 0)
            continue;

        /* Page-align the region to map */
        uint64_t va_start = phdr->p_vaddr & ~0xFFFULL;
        uint64_t va_end   = (phdr->p_vaddr + phdr->p_memsz + 0xFFFULL) & ~0xFFFULL;

        /* Build VMM flags from ELF segment permissions */
        uint64_t flags = VMM_FLAG_PRESENT | VMM_FLAG_USER;
        if (phdr->p_flags & PF_W)
            flags |= VMM_FLAG_WRITABLE;

        /* Allocate, zero, and map one page at a time */
        for (uint64_t va = va_start; va < va_end; va += 0x1000) {
            uint8_t *pg = (uint8_t *)pmm_alloc();
            for (int j = 0; j < 4096; j++) pg[j] = 0;
            vmm_map(va, (uint64_t)pg, flags);
        }

        /* Copy file bytes into the mapped virtual address */
        uint8_t       *dst = (uint8_t *)phdr->p_vaddr;
        const uint8_t *src = base + phdr->p_offset;
        for (uint64_t j = 0; j < phdr->p_filesz; j++)
            dst[j] = src[j];
        /* Remainder (BSS) already zeroed by the page allocation above */
    }

    return ehdr->e_entry;
}
