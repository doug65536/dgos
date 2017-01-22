
BUILDROOT := $(realpath ../kernel)
export BUILDROOT

ARCH ?= x86_64

LIBNAME := c
CSRCS := \
	libc/syscall/$(ARCH)/syscall6.c \
	libc/syscall/$(ARCH)/syscall5.c \
	libc/syscall/$(ARCH)/syscall4.c \
	libc/syscall/$(ARCH)/syscall3.c \
	libc/syscall/$(ARCH)/syscall2.c \
	libc/syscall/$(ARCH)/syscall1.c \
	libc/syscall/$(ARCH)/syscall0.c \
	libc/src/memcpy.c

INCS := libc/syscall libc/include
INCLUDEPATHS := $(realpath $(INCS))
INCLUDEARGS := $(patsubst %,-I%,$(INCLUDEPATHS))

CFLAGS += $(INCLUDEARGS) -O2

include $(BUILDROOT)/library.mk
