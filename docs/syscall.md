# Syscall

**Sources:** `src/syscall/syscall.h`, `src/syscall/syscall.c`, `src/syscall/syscall_entry.asm`

System calls are the interface through which ring-3 user code requests kernel services. NathanOS uses the `SYSCALL`/`SYSRET` instruction pair rather than `int 0x80` — it is faster (no IDT lookup, no TSS stack switch) and is what all modern x86-64 Linux/BSD kernels use.

---

## SYSCALL / SYSRET Mechanics

`SYSCALL` is triggered by the user with a syscall number in `RAX` and arguments in `RDI`, `RSI`, `RDX` (following the Linux x86-64 syscall ABI):

1. The CPU saves `RIP` into `RCX` and `RFLAGS` into `R11`.
2. RFLAGS is masked by `SFMASK` (clears IF and DF, so interrupts are disabled on entry).
3. CS is loaded from `STAR[47:32]` and SS from `STAR[47:32] + 8` (kernel selectors).
4. Execution jumps to `LSTAR` — the kernel's syscall entry point.

`SYSRET` is the fast return path:

1. RFLAGS is restored from `R11`.
2. RIP is restored from `RCX`.
3. CS is loaded from `STAR[63:48] + 16` and SS from `STAR[63:48] + 8` (user selectors).
4. Execution resumes in ring 3.

This is why the GDT layout requires **user data before user code** — see [gdt.md](gdt.md).

---

## MSR Configuration

`syscall_init()` programs four MSRs:

| MSR | Value | Purpose |
|-----|-------|---------|
| `EFER` (0xC0000080) | `\| SCE` (bit 0) | Enable `SYSCALL`/`SYSRET` instructions |
| `STAR` (0xC0000081) | `[47:32]=0x0008`, `[63:48]=0x0010` | Kernel CS=0x08, User base=0x10 (→ user data 0x18, code 0x20) |
| `LSTAR` (0xC0000082) | `&syscall_entry` | Entry point for `SYSCALL` |
| `SFMASK` (0xC0000084) | `IF \| DF` | RFLAGS bits to clear on `SYSCALL` entry |

The `KERNEL_GSBASE` MSR (0xC0000102) is set to `&cpu_local` so that after `swapgs` the GS segment base points at the per-CPU scratch area.

---

## Kernel Stack Switch

Unlike interrupts, `SYSCALL` does **not** automatically switch stacks. The kernel must switch from the user stack to a trusted kernel stack before doing any work. This is done in `syscall_entry.asm` using a per-CPU struct:

```c
typedef struct {
    uint64_t user_rsp;   // offset 0 — scratch for saving user RSP
    uint64_t kernel_rsp; // offset 8 — kernel stack to switch to
} __attribute__((packed)) cpu_local_t;
```

`cpu_local` is accessed via GS after `swapgs`. The layout is fixed: `user_rsp` at offset 0 and `kernel_rsp` at offset 8, because `syscall_entry.asm` uses hard-coded `[gs:0]` and `[gs:8]` offsets.

---

## `syscall_entry.asm`

```nasm
syscall_entry:
    swapgs                          ; GS now points at cpu_local
    mov [gs:0], rsp                 ; save user RSP at cpu_local.user_rsp
    mov rsp, [gs:8]                 ; load kernel RSP from cpu_local.kernel_rsp

    push rcx                        ; save user RIP (SYSCALL stored it in RCX)
    push r11                        ; save user RFLAGS

    ; Remap arguments: syscall ABI uses RDI/RSI/RDX for args 1-3
    ; syscall_dispatch(number=rax, arg1=rdi, arg2=rsi, arg3=rdx)
    mov rcx, rdx                    ; arg3
    mov rdx, rsi                    ; arg2
    mov rsi, rdi                    ; arg1
    mov rdi, rax                    ; syscall number
    call syscall_dispatch

    pop r11
    pop rcx
    mov rsp, [gs:0]                 ; restore user RSP
    swapgs
    o64 sysret                      ; return to ring 3
```

`o64 sysret` (not `sysretq`) is the correct NASM encoding for a 64-bit `SYSRET`.

---

## Syscall Dispatch

```c
uint64_t syscall_dispatch(uint64_t number, uint64_t arg1, uint64_t arg2, uint64_t arg3) {
    switch (number) {
        case SYS_WRITE: return (uint64_t)sys_write(arg1, arg2, arg3);
        case SYS_EXIT:  sys_exit(arg1);   // noreturn
    }
    return (uint64_t)-1LL;   // ENOSYS
}
```

Syscall numbers match the Linux x86-64 ABI for interoperability with the user binary:

| Number | Name | Action |
|--------|------|--------|
| `1` | `SYS_WRITE` | Writes `len` bytes from `buf` to the VGA text display (fd is ignored) |
| `60` | `SYS_EXIT` | Prints `[user exited]`, restores kernel CR3, halts |

---

## Per-Process Kernel Stack

`syscall_set_kernel_stack(rsp)` updates `cpu_local.kernel_rsp`. It is called by `process_run_elf()` immediately before dropping into ring 3 so that each process has its own dedicated kernel stack — preventing one process's syscall from corrupting another's stack frame.
