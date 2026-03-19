# Memory

**Sources:** `src/mm/pmm.c`, `src/mm/heap.c`, `src/mm/vmm.c`

The three memory managers build on each other in layers: **PMM** owns raw physical pages → **Heap** groups PMM pages into a `kmalloc`/`kfree` interface → **VMM** builds fine-grained virtual mappings on top of the identity map that [boot.asm](boot.md) established.

---

## PMM — Physical Memory Manager

**Source:** `src/mm/pmm.c`

### Concept

Physical memory is divided into 4 KB pages. The PMM tracks which pages are free with a **bitmap**: one bit per page, 0 = free, 1 = used. A first-fit linear scan finds a free page in O(n) time; allocations are rare enough that this is fine.

### Bitmap Placement

The bitmap is placed at `_kernel_end`, the symbol the linker script emits at the end of the kernel binary. This means the bitmap lives immediately after the kernel in physical memory, inside the 64 MB identity-mapped window that `boot.asm` set up.

```
Physical memory layout after pmm_init:
┌──────────────────┐ 0x00000000
│  Low 1 MB (used) │  BIOS, VGA buffer, ROM — pages 0–255 kept reserved
├──────────────────┤ 0x00100000
│   Kernel binary  │  .text, .rodata, .data, .bss, page tables, stack
├──────────────────┤ _kernel_end
│  PMM bitmap      │  ceil(total_pages / 8) bytes — scales with RAM
├──────────────────┤ first_free page (aligned up)
│  Available pages │  returned by pmm_alloc()
│       ...        │
└──────────────────┘ total RAM
```

### Initialization

```c
void pmm_init(multiboot_info_t* mbi) {
    // GRUB gives us mem_lower (below 1 MB) + mem_upper (above 1 MB), in KB
    uint64_t total_kb = mbi->mem_lower + mbi->mem_upper;
    total_pages_count = (total_kb * 1024) / PAGE_SIZE;

    bitmap = (uint8_t*)&_kernel_end;
    uint64_t bitmap_bytes = (total_pages_count + 7) / 8;

    // Mark everything as used, then free the pages above the bitmap
    memset(bitmap, 0xFF, bitmap_bytes);
    for (uint64_t i = first_free; i < total_pages_count; i++)
        bitmap_clear(i);

    // Keep low 256 pages (1 MB) reserved: BIOS, VGA, ROM
    for (uint64_t i = 0; i < 256; i++)
        bitmap_set(i);
}
```

The bitmap must fit within the 64 MB identity map. At one bit per 4 KB page, 64 MB of bitmap covers `64 * 1024 * 1024 * 8 * 4096` bytes = **~512 GB of RAM** — far more than any machine NathanOS is likely to run on.

### API

```c
void* pmm_alloc(void);       // first-fit scan; returns page base or NULL
void  pmm_free(void* page);  // panics on double-free or out-of-range
uint64_t pmm_free_pages(void);
uint64_t pmm_total_pages(void);
```

`pmm_free` panics on double-free (bitmap bit already 0) and on addresses beyond `total_pages_count`.

---

## Heap

**Source:** `src/mm/heap.c`

### Concept

The heap provides `kmalloc`/`kfree` for arbitrary-size kernel allocations. It manages a contiguous region of PMM pages using a **linked list of block headers**:

```
┌──────────────┬────────────────────────┬──────────────┬─────────────────────┐
│  block_t hdr │    payload bytes ...   │  block_t hdr │    payload ...      │
│ size|free|next│                        │ size|free|next│                     │
└──────────────┴────────────────────────┴──────────────┴─────────────────────┘
```

```c
typedef struct block {
    size_t        size;   // payload bytes (not including sizeof(block_t))
    int           free;   // 1 = available, 0 = in use
    struct block* next;   // next block in address order
} block_t;
```

### Initialization

`heap_init()` allocates `HEAP_INIT_PAGES` (16) consecutive pages from the PMM (64 KB) and creates a single free block covering the entire region. Because the PMM scans linearly, successive `pmm_alloc()` calls return adjacent pages.

### Allocation — First-Fit with Splitting

`kmalloc(size)` rounds `size` up to 16-byte alignment, then walks the list for the first free block large enough. If the block has enough room left over (at least `sizeof(block_t) + 16` bytes), it is **split**: the tail becomes a new free block.

If no suitable block is found, `heap_expand()` grabs 4 more PMM pages (`HEAP_GROW_PAGES`) and appends them to the list, then retries.

### Freeing — Coalescing on Every Free

`kfree(ptr)` locates the header by subtracting `sizeof(block_t)` from `ptr`, marks `free = 1`, then calls `coalesce()` for a single forward pass that merges adjacent free blocks. This prevents fragmentation from accumulating over time.

Double-free is detected: if `hdr->free` is already 1 when `kfree` is called, `panic(PANIC_HEAP_DOUBLE_FREE)` fires.

---

## VMM — Virtual Memory Manager

**Source:** `src/mm/vmm.c`

### Relationship to the Boot Identity Map

`boot.asm` creates a static identity map of the first 64 MB using 2 MB huge pages. This map is never torn down — the kernel and PMM bitmap live inside it and must always be accessible. The VMM does not touch those huge-page PD entries; it adds 4 KB mappings **on top** of the existing PML4 for any address outside the identity-mapped region.

### 4-Level Page Walk

x86-64 virtual addresses are split into five fields:

```
 63          48 47      39 38      30 29      21 20      12 11        0
┌─────────────┬──────────┬──────────┬──────────┬──────────┬───────────┐
│  sign extend │ PML4 idx │ PDPT idx │   PD idx │   PT idx │ page off  │
└─────────────┴──────────┴──────────┴──────────┴──────────┴───────────┘
                  9 bits     9 bits     9 bits     9 bits    12 bits
```

`vmm_map(virt, phys, flags)`:

1. Reads `CR3` to get the PML4 physical address.
2. Indexes `pml4[PML4_IDX(virt)]` — creates a PDPT page via PMM if not present.
3. Indexes `pdpt[PDPT_IDX(virt)]` — creates a PD page if not present.
4. Checks `pd[PD_IDX(virt)]` for the `PS` (huge page) bit — **panics** if set, because splitting a 2 MB boot page is not supported.
5. Indexes `pt[PT_IDX(virt)]` — creates a PT page if not present — and writes `phys | flags`.
6. Calls `invlpg(virt)` to flush the TLB entry for this address.

`get_or_alloc_table()` is the helper that lazily creates missing table levels: it calls `pmm_alloc()`, zeroes the page, and writes the entry with `VMM_KERNEL_DATA` flags.

### Flags

```c
#define VMM_FLAG_PRESENT   (1ULL << 0)
#define VMM_FLAG_WRITABLE  (1ULL << 1)
#define VMM_FLAG_USER      (1ULL << 2)   // ring 3 accessible (not yet used)
#define VMM_FLAG_PS        (1ULL << 7)   // huge page (set by boot.asm)
#define VMM_FLAG_NX        (1ULL << 63)  // no-execute (not yet used)

#define VMM_KERNEL_DATA  (VMM_FLAG_PRESENT | VMM_FLAG_WRITABLE)
#define VMM_KERNEL_CODE  (VMM_FLAG_PRESENT)
```

### `vmm_unmap`

Walks the same 4-level hierarchy and zeroes the PT entry, then calls `invlpg`. Missing levels are silently ignored (the address was never mapped). The intermediate tables (PDPT, PD, PT) are not freed back to the PMM — table reclamation is not implemented.

### Smoke Test in `kmain`

After `vmm_init()`, `kmain` exercises the VMM:

```c
uint64_t phys = (uint64_t)pmm_alloc();
uint64_t virt = 0x4001000;   // above the 64 MB identity map
vmm_map(virt, phys, VMM_KERNEL_DATA);
*(volatile uint64_t*)virt = 0xDEADBEEFCAFEBABEULL;
// read it back and verify
vmm_unmap(virt);
pmm_free((void*)phys);
```

A write through the new virtual address and a matching readback confirm that the page table walk is correct end-to-end.
