#ifndef PROC_H
#define PROC_H

/*
 * process_run_elf — create an isolated address space, load the ELF64
 * executable at `elf_data` into it, set up a per-process kernel stack,
 * and drop into ring 3.  Does not return.
 */
__attribute__((noreturn)) void process_run_elf(const void *elf_data);

#endif
