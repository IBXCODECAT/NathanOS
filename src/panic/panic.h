#ifndef PANIC_H
#define PANIC_H

#include <stdint.h>

// Single source of truth for every panic code.
// Adding a new code here automatically adds it to the enum and string table.
#define PANIC_CODE_LIST \
    X(DIVIDE_BY_ZERO)                   \
    X(DEBUG_EXCEPTION)                  \
    X(NON_MASKABLE_INTERRUPT)           \
    X(BREAKPOINT_TRAP)                  \
    X(OVERFLOW)                         \
    X(BOUND_RANGE_EXCEEDED)             \
    X(INVALID_OPCODE)                   \
    X(DEVICE_NOT_AVAILABLE)             \
    X(DOUBLE_FAULT)                     \
    X(COPROCESSOR_SEGMENT_OVERRUN)      \
    X(INVALID_TSS)                      \
    X(SEGMENT_NOT_PRESENT)              \
    X(STACK_SEGMENT_FAULT)              \
    X(GENERAL_PROTECTION_FAULT)         \
    X(PAGE_FAULT_IN_NON_PAGED_AREA)     \
    X(RESERVED_EXCEPTION)               \
    X(X87_FLOATING_POINT_ERROR)         \
    X(ALIGNMENT_CHECK)                  \
    X(MACHINE_CHECK)                    \
    X(SIMD_FLOATING_POINT_EXCEPTION)    \
    X(VIRTUALIZATION_EXCEPTION)         \
    X(CONTROL_PROTECTION_EXCEPTION)     \
    X(HYPERVISOR_INJECTION_EXCEPTION)   \
    X(VMM_COMMUNICATION_EXCEPTION)      \
    X(SECURITY_EXCEPTION)               \
    X(INVALID_IRQ_NUMBER)               \
    X(NO_MULTIBOOT_INFO)                \
    X(PMM_NULL_MULTIBOOT_INFO)          \
    X(PMM_MULTIBOOT_MEM_UNAVAILABLE)    \
    X(PMM_NO_MEMORY_DETECTED)           \
    X(PMM_BITMAP_EXCEEDS_MAPPED_REGION) \
    X(PMM_FREE_OUT_OF_RANGE)            \
    X(PMM_DOUBLE_FREE)                  \
    X(MANUALLY_INITIATED_PANIC)

typedef enum {
#define X(name) PANIC_##name,
    PANIC_CODE_LIST
#undef X
    PANIC_CODE_COUNT
} panic_code_t;

void panic(panic_code_t code);
void panic_ex(panic_code_t code, uint64_t rip, uint64_t err, uint64_t fault_addr);

#endif
