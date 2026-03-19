# NathanOS

A hobby x86-64 operating system built from scratch as a learning opportunity for understanding how computers actually work — from the first instruction a CPU executes after power-on, through memory management, interrupt handling, and preemptive multitasking, all the way down to the hardware.

No libc. No external runtime. Just C, NASM assembly, and the bare metal.

## Documentation

**[docs/index.md](docs/index.md)** — start here for a full project overview, the kernel startup sequence, and links to every subsystem.

## Building

```sh
./build.sh
```

Requires `nasm`, `gcc`, `ld`, `grub-mkrescue`, and `qemu-system-x86_64`.
