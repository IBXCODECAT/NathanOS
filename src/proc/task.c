#include "task.h"
#include "../mm/pmm.h"
#include "../mm/heap.h"
#include "../drivers/timer.h"
#include "../panic/panic.h"
#include "../cpu.h"

task_t* current_task = 0;

static task_t*  run_queue = 0;
static uint32_t next_id   = 0;

// Entry point for every new task. Re-enables interrupts (they are off because
// we arrive here via switch_to, which was called from inside the timer IRQ),
// then calls the real entry function. If the entry function ever returns the
// task is marked dead and yields forever.
static void schedule_tick(uint64_t ticks);

static void task_trampoline(void) {
    sti();
    current_task->entry();
    current_task->state = TASK_DEAD;
    for (;;) schedule();
}

void sched_init(void) {
    task_t* boot = kmalloc(sizeof(task_t));
    if (!boot) panic(PANIC_OUT_OF_MEMORY);

    boot->rsp   = 0;           // filled by switch_to on first preemption
    boot->entry = 0;
    boot->stack = 0;           // boot stack — not PMM-managed
    boot->state = TASK_RUNNING;
    boot->id    = next_id++;
    boot->next  = boot;        // circular: only task so far

    current_task = boot;
    run_queue    = boot;

    timer_register_tick_callback(schedule_tick);
}

// Called by the timer driver on every tick (signature matches the callback type)
void schedule_tick(uint64_t ticks) {
    (void)ticks;
    schedule();
}

task_t* task_create(void (*entry)(void)) {
    task_t* t = kmalloc(sizeof(task_t));
    if (!t) panic(PANIC_OUT_OF_MEMORY);

    uint8_t* stack = pmm_alloc();
    if (!stack) panic(PANIC_OUT_OF_MEMORY);

    // Build the initial stack frame that switch_to expects:
    //   [sp+40] = task_trampoline  ← ret will jump here
    //   [sp+32] = 0                ← r15
    //   [sp+24] = 0                ← r14
    //   [sp+16] = 0                ← r13
    //   [sp+8]  = 0                ← r12
    //   [sp+0]  = 0                ← rbp  (rbx is above trampoline addr,
    //   ... wait, push order is rbx,rbp,r12..r15 so pop is r15..r12,rbp,rbx)
    // switch_to pushes: rbx rbp r12 r13 r14 r15  → stack grows down
    // switch_to pops:   r15 r14 r13 r12 rbp rbx  → then ret
    // So initial stack (top to bottom in memory, rsp points to rbx slot):
    //   [rsp+48] = task_trampoline
    //   [rsp+40] = r15 = 0
    //   [rsp+32] = r14 = 0
    //   [rsp+24] = r13 = 0
    //   [rsp+16] = r12 = 0
    //   [rsp+8]  = rbp = 0
    //   [rsp+0]  = rbx = 0   ← task->rsp points here
    uint64_t* sp = (uint64_t*)(stack + 4096);
    *--sp = (uint64_t)task_trampoline;  // return address
    *--sp = 0;  // r15
    *--sp = 0;  // r14
    *--sp = 0;  // r13
    *--sp = 0;  // r12
    *--sp = 0;  // rbp
    *--sp = 0;  // rbx

    t->rsp   = (uint64_t)sp;
    t->entry = entry;
    t->stack = stack;
    t->state = TASK_READY;
    t->id    = next_id++;

    // Insert after current_task in the circular list
    t->next            = current_task->next;
    current_task->next = t;

    return t;
}

void schedule(void) {
    task_t* next = current_task->next;

    // Skip dead tasks
    while (next->state == TASK_DEAD && next != current_task)
        next = next->next;

    if (next == current_task) return;

    current_task->state = TASK_READY;
    next->state         = TASK_RUNNING;
    switch_to(&current_task, next);
}
