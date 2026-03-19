# NathanOS

NathanOS is an x86-64 hobby operating system written from scratch in C and NASM assembly. It boots via GRUB (Multiboot), transitions from 32-bit protected mode to 64-bit long mode, and runs a preemptive round-robin scheduler — all without libc or any external runtime. The project exists to understand how a real OS works at every layer, from the first instruction GRUB hands control to through physical memory allocation, virtual address mapping, interrupt handling, and context switching.

## Startup Sequence

Every init call in `kmain` (`src/krnl.c`), in order:

| # | Call | Why |
|---|------|-----|
| 1 | `vga_init()` | Clears the VGA text buffer and sets the default color so all subsequent output has somewhere to go. Must be first. |
| 2 | `idt_init()` | Remaps the 8259 PIC and loads all 48 IDT gate descriptors. Without this, hardware interrupts and CPU exceptions have no handler. |
| 3 | `pmm_init(mbi)` | Reads GRUB's memory map to discover usable RAM, then builds a bitmap allocator. Every subsystem that needs memory calls the PMM. |
| 4 | `heap_init()` | Claims 16 PMM pages (64 KB) and sets up the `kmalloc`/`kfree` linked-list heap used by the scheduler and other kernel code. |
| 5 | `vmm_init()` | Validates CR3 and installs the VMM on top of boot's identity map. Enables fine-grained 4KB page mappings for anything outside the first 64 MB. |
| 6 | `timer_init()` | Programs the 8253/8254 PIT to fire IRQ 0 at 100 Hz. The scheduler and keyboard driver both depend on this tick source. |
| 7 | `keyboard_init()` | Registers the PS/2 IRQ 1 handler and the Esc-hold timer callback. |
| 8 | `sched_init()` | Wraps the boot context in a `task_t`, registers `schedule_tick` as the timer callback, then `task_create` adds the first user tasks. |

After init, `kmain` calls `sti()` to unmask interrupts and loops on `hlt()`. From that point the CPU is driven entirely by timer and keyboard IRQs.

## Subsystem Pages

- [Boot](boot.md) — real→long mode, page tables, GDT
- [Interrupts](interrupts.md) — IDT, ISR stubs, PIC remapping
- [Memory](memory.md) — PMM, heap, VMM
- [Drivers](drivers.md) — VGA, keyboard, timer
- [Scheduler](scheduler.md) — preemptive multitasking, context switch
- [Panic](panic.md) — BSOD handler, X-macro panic codes
- [Roadmap](roadmap.md) — ring 3, syscalls, shell, and beyond

## Building and Running

```sh
./build.sh
```

`build.sh` does the following in order:

1. Assembles `boot.asm`, `idt/isr.asm`, and `proc/switch.asm` with NASM into `elf64` objects.
2. Compiles every `.c` file under `src/` with GCC (`-ffreestanding -fno-pie -mcmodel=kernel`).
3. Links everything with `ld` using `src/linker.ld`, producing the `krnl` binary.
4. Packages `krnl` into a GRUB rescue ISO (`nOS.iso`) with `grub-mkrescue`.
5. Launches the ISO in QEMU: `qemu-system-x86_64 -cdrom nOS.iso`.

Dependencies: `nasm`, `gcc`, `ld`, `grub-mkrescue`, `qemu-system-x86_64`.
