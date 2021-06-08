$(info x86_64 architecture specifics)

KERNEL_ARCH_SOURCES_SHARED =

bootefi_SOURCES += \
    boot/aarch64/arch_cpu.cc \
    boot/aarch64/arch_paging.cc
