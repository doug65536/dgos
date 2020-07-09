<!-- [![](https://api.travis-ci.org/doug65536/dgos.svg?branch=master)](https://travis-ci.org/doug65536/dgos) -->

- x86-64 platform

## Bootloader

- Can boot from hard disk, flash drive, or CD/DVD using BIOS or EFI
- Either MBR -> bootloader in partition boot sector reserved area,
  or GPT boot to EFI system partition (GPT hybrid format, working MBR present)
- BIOS boot runs in 32 bit protected mode, EFI boot runs in 64 bit long mode
- Implemented mostly in C++
- Custom bootloader, supports FAT32 with LFN and ISO9660 with Joliet
  boot with unified booloader
- Filesystem accesses abstracted to use own filesystem implementation
  and BIOS calls for BIOS boot, or use EFI provided filesystem APIs for EFI
- Builds page tables and enters ELF64 kernel with CPU fully in long mode
- Implements SMP trampoline
- Gets physical memory map from BIOS or EFI
- Uses memory map throughout, can tolerate arbitrarily fragmented memory map
- Memory map is handed over to kernel so all bootloader allocations are
  accounted in the map
- Enumerates VESA BIOS video modes and configures video card for kernel
- Implements basic exception handlers
- Shows loading progress bar
- Processes position independent executable relocations, allowing the
  kernel to be loaded to an arbitrary address for kernel address space
  layout randomization.
- Implements boot menu
- Abstracts screen output and keyboard input so it can use BIOS
  or EFI accordingly

## Kernel

- SMP (supporting Multiprocessor Specification and ACPI)
- LAPIC timer driven preemptive multithreading
- Priority-based scheduler
- Tickless scheduler, variable deadline at timeslice exhaustion or timer expiry
- Processor affinity
- Oldest timeslice first, optimizes latency over throughput. Timeslice
  exhaustion causes the repleished timeslice to become young again.
- Full SSE/AVX/AVX2/AVX-512 support using
  fxsave/fxrstor or xsave/xsavec/xsaveopt/xrstor where available
- Recursive paging
- Per-CPU small block heap
- Position independent executable memory model with address randomization
- Demand paged memory
- Lazy TLB shootdown
- Memory protection (no mapping of entire physical memory)
- Interprocessor TLB shootdown
- High resolution sleep and usleep implementation
- RTC
- Atomics
- GSBASE based CPU-local storage
- FSBASE and GSBASE per user thread
- Interrupt Stack Table support with emergency stacks for critical exceptions
- MCS locks, Spinlocks, Reader/Writer Spinlocks
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
- virtio-block disk driver
- virtio-gpu driver with dynamic vm window resize handling
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
- Instrumented call trace generation with ncurses viewer
- KASAN address sanitizer
- UBSAN (undefined behavior sanitizer)
- All filesystems, partition parsers, and device drivers in separate
  dynamically loaded modules
- Kernel unit test module (for unit testing parts of the kernel)
- PNG parser
- Fast system memory to video memory clipped blitter with AVX, SSE4.1, SSE2
  optimizations and a platform independent implementation
