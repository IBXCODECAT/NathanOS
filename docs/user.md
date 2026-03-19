# User Mode

**Sources:** `src/elf/elf.h`, `src/elf/elf.c`, `src/proc/proc.h`, `src/proc/proc.c`, `src/user/user.asm`

User mode encompasses everything needed to run an untrusted program in ring 3: an ELF64 loader, a user binary, and the process creation code that ties them together.

---

## User Binary (`src/user/user.asm`)

The user binary is a hand-written NASM ELF64 program linked at virtual address `0x8000000`. It uses Linux-compatible syscall numbers so no libc is needed:

```nasm
section .text
global _start
_start:
    ; SYS_WRITE(fd=1, buf=msg, len=msg_len)
    mov rax, 1          ; SYS_WRITE
    mov rdi, 1          ; fd (stdout — ignored by kernel, writes to VGA)
    lea rsi, [rel msg]  ; RIP-relative address of message
    mov rdx, msg_len
    syscall

    ; SYS_EXIT(0)
    mov rax, 60         ; SYS_EXIT
    xor rdi, rdi
    syscall
```

`[rel msg]` is essential: without RIP-relative addressing, the assembler would embed an absolute 32-bit address, which would be wrong once the binary is loaded at `0x8000000`.

### Build Process

```sh
# 1. Assemble to a relocatable object (intermediate — goes to /tmp to avoid
#    being picked up by the kernel linker's `find src/ -name "*.o"`)
nasm -f elf64 src/user/user.asm -o /tmp/nOS_user_stage.o

# 2. Link into a standalone ELF64 at the load address
ld -Ttext=0x8000000 --oformat=elf64-x86-64 /tmp/nOS_user_stage.o -o src/user/user.elf

# 3. Embed the ELF as a raw byte array in the kernel binary
objcopy -I binary -O elf64-x86-64 -B i386:x86-64 src/user/user.elf src/user/user_elf.o
```

`objcopy -I binary` creates three linker symbols in `user_elf.o`:
- `_binary_user_elf_start` — pointer to the first byte of `user.elf`
- `_binary_user_elf_end` — pointer past the last byte
- `_binary_user_elf_size` — size as an absolute symbol

The kernel accesses the embedded ELF via `extern uint8_t _binary_user_elf_start[]`.

---

## ELF64 Loader (`src/elf/elf.c`)

`elf_load(data)` loads PT_LOAD segments from an in-memory ELF64 binary into the **current address space** (whichever CR3 is active when it is called).

### ELF64 Header Validation

```c
// Magic: 0x7F 'E' 'L' 'F'
// EI_CLASS = 2 (64-bit)
// e_type    = ET_EXEC (2)
// e_machine = EM_X86_64 (62)
```

Any mismatch panics with `PANIC_ELF_INVALID`.

### Loading PT_LOAD Segments

For each program header with `p_type == PT_LOAD`:

1. **Page-align** the virtual address range: `va_start = p_vaddr & ~0xFFF`, `va_end = (p_vaddr + p_memsz + 0xFFF) & ~0xFFF`.
2. For each 4 KB page in `[va_start, va_end)`:
   - `pmm_alloc()` a fresh physical page.
   - `memset` it to zero (handles BSS — the region between `p_filesz` and `p_memsz`).
   - `vmm_map(virt, phys, flags)` — flags are derived from the segment's `p_flags` (PF_W → WRITABLE, PF_X → no NX, PF_U → USER).
3. `memcpy` the `p_filesz` bytes from the ELF data into the mapped virtual address.

The zero-then-copy order means the BSS is automatically zeroed: pages beyond `p_filesz` are zero-filled, and the partial page at the `p_filesz` boundary is also clean.

### Return Value

`elf_load` returns `ehdr->e_entry` — the ELF entry point virtual address, which `process_run_elf` passes to `ring3_enter`.

---

## Process Creation (`src/proc/proc.c`)

```c
__attribute__((noreturn)) void process_run_elf(const void *elf_data) {
    /* 1. Create an isolated address space */
    uint64_t as = vmm_create_address_space();

    /* 2. Allocate a dedicated kernel stack for this process.
          Both TSS.rsp0 (interrupt entry) and cpu_local.kernel_rsp (SYSCALL entry)
          must point into it so privilege transitions always land on a trusted stack. */
    uint8_t *kstack     = (uint8_t *)pmm_alloc();
    uint64_t kstack_top = (uint64_t)(kstack + 4096);
    gdt_set_kernel_stack(kstack_top);
    syscall_set_kernel_stack(kstack_top);

    /* 3. Switch into the new address space and load the ELF.
          vmm_map() calls inside elf_load() now populate the process page tables. */
    vmm_switch(as);
    uint64_t entry = elf_load(elf_data);

    /* 4. Map one page for the user stack */
    uint8_t *ustack = (uint8_t *)pmm_alloc();
    vmm_map(USER_STACK_BASE, (uint64_t)ustack, VMM_USER_DATA);

    /* 5. Drop into ring 3 — CR3 is already set to `as` */
    ring3_enter(entry, USER_STACK_BASE + 0x1000);
}
```

### Memory Layout After Process Creation

```
Virtual address space (per-process):
┌────────────────────────┐ 0x0000000000000000
│  Kernel identity map   │  0x00000000 – 0x04000000 (64 MB huge pages, copied from kernel PD)
│  (readable from ring 0)│
├────────────────────────┤ 0x0000000004000000
│  (unmapped)            │
├────────────────────────┤ 0x0000000004010000  ← USER_STACK_BASE
│  User stack (1 page)   │  RSP starts at +0x1000 (top of page, stack grows down)
├────────────────────────┤ 0x0000000004011000
│  (unmapped)            │
├────────────────────────┤ 0x0000000008000000
│  ELF text/rodata       │  PT_LOAD segments from user.elf (linked at 0x8000000)
│  ELF data/bss          │
└────────────────────────┘
```

### Kernel Stack vs User Stack

| Stack | Location | Used by |
|-------|----------|---------|
| Kernel stack (`kstack`) | PMM page, kernel virtual address | Interrupt handlers, syscall handler while in ring 0 |
| User stack (`ustack`) | PMM page, mapped at `USER_STACK_BASE` | User-mode code (RSP points here on `iretq`) |

The kernel and user stacks are completely separate. The kernel never uses the user stack during privilege transitions — it always switches to `kstack` via `TSS.rsp0` (interrupts) or `cpu_local.kernel_rsp` (syscalls).
