[![](https://api.travis-ci.org/doug65536/dgos.svg?branch=master)](https://travis-ci.org/doug65536/dgos)

- x86-64 platform

## Bootloader

- Implemented mostly in C++
- Custom bootloader, supports FAT32 with LFN and ISO9660 with Joliet
  boot with unified booloader
- Can boot from hard disk, CD/DVD, or flash drive
- Builds page tables and enters ELF64 kernel with CPU fully in long mode
- Implements SMP trampoline
- Gets physical memory map from BIOS
- Enumerates VESA BIOS video modes and configures video card for kernel
- Implements basic exception handlers
- Shows loading progress bar

## Kernel

- SMP (supporting Multiprocessor Specification and ACPI)
- Priority-based scheduler
- LAPIC timer driven preemptive multithreading
- Processor affinity
- full SSE/AVX/AVX2/AVX-512 support using
  fxsave/fxrstor or xsave/xsavec/xsaveopt/xrstor where available
- Super fast recursive paging implementation
- Super fast small block heap implementation
- High-half mcmodel=kernel memory model
- Demand paged memory
- Lazy TLB shootdown
- Memory protection (no mapping of entire physical memory, no identity mapping)
- Interprocessor TLB shootdown
- Sleep with 16ms resolution and usleep implementation
- RTC
- Atomics
- GSBASE based CPU-local storage
- FSBASE per thread
- Interrupt Stack Table support with emergency stacks for critical exceptions
- Spinlocks, Reader/Writer Spinlocks
- Mutex, Condition Variable, Reader/Writer locks
- 8259 or LAPIC/IO-APIC support, multiple IO-APICs supported
- PCI MSI IRQ support including multiple message capability
- PCI MSI-X IRQ support including routing IRQs to arbitrary CPUs
- PCIe ECAM (enhanced configuration access method) MMIO configuration space
- IRQ sharing
- Fast PCI enumerator with recursive bridge traversal
- 8042 legacy keyboard and mouse driver
- XHCI USB 3.1 driver
- USB Mouse, Keyboard support
- USB mass storage support (for flash drives and external hard drives)
- IRQ driven IDE with bus master DMA support, IORDY, 48-bit addressing, ATAPI,
  bounce buffering
- IRQ driven AHCI with native command queueing (NCQ), asynchronous I/O,
  handles up to 32 concurrent commands per port
- IRQ driven NVME with per-cpu queues and multiple namespace support
- FAT32 and ISO9660 filesystems with LFN and Joliet support
  (with full unicode support, including outside the basic multilingual plane)
- Virtual block storage device abstraction layer
- Abstract partition probe with MBR and ISO9660 implementations
- VFS filesystem absraction layer
- Generalized I/O device page cache with writeback policy and msync flush
- Dynamically loaded kernel modules with runtime relocation and
  linkage of kernel exports
- Non-temporal memory operations
- SSE/AVX2 optimized memory operations
- Framebuffer with offscreen shadow buffer and clipping blitter
- Smooth subpixel precise line drawing algorithm
- Linux compatible syscall interface
- Try/Catch unhandled exception hooking
- Port 0xE9 based debug output
- Abstract NIC interface layer
- RTL8139 NIC driver
- Ethernet/ARP/UDP/ICMP/DHCP/IPv4 network implemented (so far)
- Full-featured printf formatter including floating point support
- High detail exception information written to debug console
- Several stress test implementations
- Extensive debugging support for QEMU and Bochs
- QtCreator project files for easy editing and kernel debugging
