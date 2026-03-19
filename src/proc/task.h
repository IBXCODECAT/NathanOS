#ifndef TASK_H
#define TASK_H

#include <stdint.h>

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_DEAD,
} task_state_t;

typedef struct task task_t;
struct task {
    uint64_t      rsp;        // MUST be first — switch.asm accesses at offset 0
    void        (*entry)(void);
    uint8_t*      stack;      // PMM page base (NULL for the boot task)
    task_state_t  state;
    uint32_t      id;
    task_t*       next;       // circular linked list
};

extern task_t* current_task;

void    sched_init(void);
task_t* task_create(void (*entry)(void));
void    schedule(void);

// Assembly: void switch_to(task_t** current_ptr, task_t* next)
void switch_to(task_t** current_ptr, task_t* next);

#endif
