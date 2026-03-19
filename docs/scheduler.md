# Scheduler

> **Note:** The original ring-0 preemptive round-robin scheduler has been removed as the kernel transitions to proper ring-3 user-mode processes. Process management is now handled through `src/proc/proc.c`. This page documents both the old design (for historical reference) and the current process model.

---

## Current Model — Ring-3 Single Process

**Source:** `src/proc/proc.c`

The kernel currently supports one user-mode process at a time. `process_run_elf()` is responsible for the full lifecycle of creating and launching it:

1. `vmm_create_address_space()` — allocates an isolated PML4/PDPT/PD for the process.
2. `pmm_alloc()` — allocates a dedicated **kernel stack** (one 4 KB page). Both `TSS.rsp0` and `cpu_local.kernel_rsp` are pointed at its top so that both interrupt-entry and `SYSCALL`-entry land on this stack.
3. `vmm_switch(as)` — switches CR3 to the new address space.
4. `elf_load(elf_data)` — parses the embedded ELF64 binary and maps its segments into the new address space (see [user.md](user.md)).
5. `pmm_alloc()` + `vmm_map()` — allocates and maps one page for the user stack at `USER_STACK_BASE` (0x4010000) with `VMM_USER_DATA` flags.
6. `ring3_enter(entry, USER_STACK_BASE + 0x1000)` — builds an `iretq` frame on the kernel stack and drops to ring 3.

The kernel stack is idle after `ring3_enter`. When the user process makes a syscall or triggers an interrupt, the CPU switches back to this kernel stack automatically (via `TSS.rsp0` / `cpu_local.kernel_rsp`).

When the process calls `SYS_EXIT`, `sys_exit()` restores the kernel's CR3 via `vmm_switch(vmm_kernel_cr3())` and halts.

---

## Historical Design — Ring-0 Preemptive Scheduler

The following documents the ring-0 round-robin scheduler that existed before ring-3 support was added. It is preserved here as a reference for how context switching works.

### `task_t` Structure

```c
struct task {
    uint64_t      rsp;        // MUST be first — switch.asm accesses at offset 0
    void        (*entry)(void);
    uint8_t*      stack;      // PMM page base (NULL for the boot task)
    task_state_t  state;      // TASK_READY, TASK_RUNNING, or TASK_DEAD
    uint32_t      id;
    task_t*       next;       // circular linked list
};
```

`rsp` being at offset 0 is not a convention choice — `switch.asm` uses `mov [rax], rsp` and `mov rsp, [rsi]` to read/write it directly by pointer, with no struct-offset arithmetic.

### `switch_to` — The Context Switch

`switch_to(task_t** current_ptr, task_t* next)` saved/restored the six callee-saved registers (rbx, rbp, r12–r15) on the outgoing task's stack and loaded the incoming task's stack. The `ret` at the end of the function resumed wherever the incoming task was suspended.

### Why New Tasks Needed `sti()`

New tasks entered `task_trampoline` with interrupts **disabled** because `switch_to` was called from inside the timer IRQ handler (IF=0). The `iretq` that would normally re-enable interrupts never ran for the new task — it continued from the `ret` in `switch_to`. An explicit `sti()` at the top of the trampoline re-enabled preemption.

### Round-Robin Scheduling

`schedule()` walked forward one step in the circular task list, skipping `TASK_DEAD` tasks. All tasks got equal 10 ms time slices driven by IRQ 0 at 100 Hz.

### EOI-Before-Handler Dependency

The scheduler worked correctly because the IDT layer sent EOI to the PIC **before** calling the timer IRQ handler (see [interrupts.md](interrupts.md)). If EOI were sent after the handler returned, the context switch inside `schedule()` would leave the PIC waiting for an EOI that never arrived, blocking all future IRQ 0 interrupts.
