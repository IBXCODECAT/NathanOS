# NathanOS

NathanOS is an x86-64 hobby operating system written from scratch in C and NASM assembly. It boots via GRUB (Multiboot), transitions from 32-bit protected mode to 64-bit long mode, and runs a user-mode ELF binary in ring 3 — all without libc or any external runtime. The project exists to understand how a real OS works at every layer, from the first instruction GRUB hands control to through physical memory allocation, virtual address mapping, interrupt handling, privilege separation, and system calls.

## Startup Sequence

Every init call in `kmain` (`src/krnl.c`), in order:

| # | Call | Why |
|---|------|-----|
| 1 | `vga_init()` | Clears the VGA text buffer and sets the default color so all subsequent output has somewhere to go. Must be first. |
| 2 | `idt_init()` | Remaps the 8259 PIC and loads all 48 IDT gate descriptors. Without this, hardware interrupts and CPU exceptions have no handler. |
| 3 | `pmm_init(mbi)` | Reads GRUB's memory map to discover usable RAM, then builds a bitmap allocator. Every subsystem that needs memory calls the PMM. |
| 4 | `heap_init()` | Claims 16 PMM pages (64 KB) and sets up the `kmalloc`/`kfree` linked-list heap used by kernel code. |
| 5 | `vmm_init()` | Validates CR3, saves the kernel's CR3, and installs the VMM on top of boot's identity map. Enables fine-grained 4 KB page mappings and per-process address spaces. |
| 6 | `gdt_init()` | Replaces the boot GDT with a full 7-descriptor table: null, kernel code/data, user data/code, and a 64-bit TSS. Loads the TSS so the CPU knows where to find the kernel stack when an interrupt fires in ring 3. |
| 7 | `syscall_init()` | Enables the `SYSCALL`/`SYSRET` instructions via the EFER MSR and programs STAR/LSTAR/SFMASK. Sets up the per-CPU `cpu_local` struct (accessed via GS after `swapgs`) for fast kernel-stack switching on syscall entry. |
| 8 | `process_run_elf(...)` | Creates an isolated address space, loads the embedded ELF64 user binary into it, allocates a per-process kernel stack, and drops into ring 3 via `iretq`. Does not return. |

After `process_run_elf` the kernel stack is idle; the CPU runs entirely in the user binary until it calls `SYS_EXIT`.

## Subsystem Pages

- [Boot](boot.md) — real→long mode, page tables, boot GDT
- [Interrupts](interrupts.md) — IDT, ISR stubs, PIC remapping
- [Memory](memory.md) — PMM, heap, VMM, per-process address spaces
- [GDT & TSS](gdt.md) — runtime GDT, TSS, ring3_enter
- [Syscall](syscall.md) — SYSCALL/SYSRET, MSR setup, dispatch
- [User Mode](user.md) — ELF64 loader, user binary, process creation
- [Drivers](drivers.md) — VGA, keyboard, timer
- [Panic](panic.md) — BSOD handler, X-macro panic codes

## Building and Running

```sh
./build.sh
```

`build.sh` does the following in order:

1. Assembles `boot.asm`, `idt/isr.asm`, `gdt/gdt_flush.asm`, and `syscall/syscall_entry.asm` with NASM into `elf64` objects.
2. Assembles `src/user/user.asm` into a standalone ELF64 binary (`user.elf`) linked at `0x8000000`, then wraps it with `objcopy -I binary` so the kernel can embed it as a raw byte array.
3. Compiles every `.c` file under `src/` with GCC (`-ffreestanding -fno-pie -mcmodel=kernel`).
4. Links everything with `ld` using `src/linker.ld`, producing the `krnl` binary.
5. Packages `krnl` into a GRUB rescue ISO (`nOS.iso`) with `grub-mkrescue`.
6. Launches the ISO in QEMU: `qemu-system-x86_64 -cdrom nOS.iso`.

Dependencies: `nasm`, `gcc`, `ld`, `grub-mkrescue`, `qemu-system-x86_64`.
