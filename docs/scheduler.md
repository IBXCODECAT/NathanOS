# Scheduler

**Sources:** `src/proc/task.h`, `src/proc/task.c`, `src/proc/switch.asm`

## Why Preemptive?

A cooperative scheduler requires every task to explicitly yield. If a task loops without yielding — or worse, hangs — the whole system stalls. A preemptive scheduler forces a context switch on every timer tick regardless of what the running task is doing. NathanOS uses the 100 Hz PIT timer (10 ms ticks) as the preemption source. Every 10 ms the CPU takes an IRQ 0 interrupt, and if multiple tasks are ready, a different one is switched in.

## `task_t` Structure

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

The tasks form a **circular singly-linked list**. `run_queue` points at the head; `schedule()` always walks forward from `current_task`.

## `switch_to` — The Context Switch

`switch_to(task_t** current_ptr, task_t* next)` is a pure-assembly function in `src/proc/switch.asm`. It follows the System V AMD64 calling convention:

- `RDI` = `&current_task` (pointer to the global pointer)
- `RSI` = `next` (the task to switch to)

```nasm
switch_to:
    ; Save outgoing task's callee-saved registers onto its stack
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; current_task->rsp = rsp
    mov rax, [rdi]       ; rax = current_task
    mov [rax], rsp       ; save RSP at offset 0

    ; current_task = next
    mov [rdi], rsi

    ; rsp = next->rsp
    mov rsp, [rsi]

    ; Restore incoming task's callee-saved registers
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ret                  ; resumes wherever next was suspended
```

Only the six callee-saved registers (rbx, rbp, r12–r15) need to be saved. The caller-saved registers (rax, rcx, rdx, rsi, rdi, r8–r11) are the responsibility of whatever code *called* `switch_to` — which is `schedule()`, a C function that the compiler has already saved and restored as needed.

## Initial Stack Frame for New Tasks

A brand-new task has never run, so it has no saved registers on its stack yet. `task_create` builds a fake frame that `switch_to` will "restore" on the first switch-in:

```
High address (stack bottom + 4096)
  [rsp+48] task_trampoline   ← return address that `ret` in switch_to will jump to
  [rsp+40] 0                 ← r15
  [rsp+32] 0                 ← r14
  [rsp+24] 0                 ← r13
  [rsp+16] 0                 ← r12
  [rsp+8]  0                 ← rbp
  [rsp+0]  0                 ← rbx   ← task->rsp points here
Low address
```

Seven 8-byte slots: six zeroed callee-saved register values and the trampoline address as the return address. When `switch_to` pops r15..rbx and executes `ret`, it jumps to `task_trampoline`.

## `task_trampoline` — Why New Tasks Need `sti()`

```c
static void task_trampoline(void) {
    sti();
    current_task->entry();
    current_task->state = TASK_DEAD;
    for (;;) schedule();
}
```

New tasks enter `task_trampoline` with interrupts **disabled**. This happens because `switch_to` was called from inside the timer IRQ handler, which runs with `IF=0` (interrupt gates clear the interrupt flag). The `iretq` that would normally re-enable interrupts never executes for the new task — it continues from the `ret` in `switch_to`. Without `sti()`, the new task would run forever with no timer ticks and never be preempted.

If the task's entry function ever returns, the task is marked `TASK_DEAD` and calls `schedule()` in a loop to yield permanently.

## `sched_init` — Wrapping the Boot Context

```c
void sched_init(void) {
    task_t* boot = kmalloc(sizeof(task_t));
    boot->rsp   = 0;          // filled by switch_to on first preemption
    boot->state = TASK_RUNNING;
    boot->next  = boot;       // circular: only task so far

    current_task = boot;
    run_queue    = boot;

    timer_register_tick_callback(schedule_tick);
}
```

The boot execution context (the `kmain` stack) is wrapped in a heap-allocated `task_t` so the scheduler can treat it like any other task. Its `rsp` is 0 initially — `switch_to` fills it with the real RSP the first time `kmain`'s context is preempted. `stack` is `NULL` because the boot stack is not a PMM-managed page.

## Round-Robin Scheduling

`schedule()` walks forward one step from `current_task` in the circular list, skipping `TASK_DEAD` tasks:

```c
void schedule(void) {
    task_t* next = current_task->next;
    while (next->state == TASK_DEAD && next != current_task)
        next = next->next;
    if (next == current_task) return;   // only one runnable task
    current_task->state = TASK_READY;
    next->state         = TASK_RUNNING;
    switch_to(&current_task, next);
}
```

This is a pure round-robin with no priorities. All tasks get equal time slices of 10 ms.

## EOI-Before-Handler Dependency

The scheduler only works correctly because the IDT layer sends EOI to the PIC **before** calling the timer IRQ handler (see [interrupts.md](interrupts.md)). If EOI were sent after the handler returned, the context switch inside `schedule()` would leave the PIC waiting for an EOI that never arrives, blocking all future IRQ 0 interrupts and freezing the scheduler after the very first switch.
