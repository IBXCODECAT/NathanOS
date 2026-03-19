; switch_to(task_t** current_ptr, task_t* next)
;   RDI = pointer to the `current_task` global (task_t**)
;   RSI = pointer to the next task_t
;
; task_t layout (task.h):
;   offset 0: rsp   (uint64_t) — MUST stay first

global switch_to
switch_to:
    ; Save callee-saved registers of the outgoing task onto its stack
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15

    ; Store current RSP into (*current_ptr)->rsp  (offset 0)
    mov rax, [rdi]          ; rax = *current_ptr  (= current task_t*)
    mov [rax], rsp          ; current_task->rsp = rsp

    ; Advance the current_task global to `next`
    mov [rdi], rsi          ; *current_ptr = next

    ; Load next task's saved RSP
    mov rsp, [rsi]          ; rsp = next->rsp

    ; Restore callee-saved registers of the incoming task
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx

    ; Return into wherever the next task was suspended
    ; (or into task_trampoline for a brand-new task)
    ret
