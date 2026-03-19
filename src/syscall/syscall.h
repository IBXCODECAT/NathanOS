#ifndef SYSCALL_H
#define SYSCALL_H
#include <stdint.h>

#define SYS_WRITE   1
#define SYS_EXIT    60

void syscall_init(void);
void syscall_set_kernel_stack(uint64_t rsp);

#endif
