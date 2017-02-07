
BUILDROOT ?= $(realpath ../../kernel)

ARCH ?= x86_64

LIBNAME := c
CSRCS := \
	syscall/$(ARCH)/syscall6.c \
	syscall/$(ARCH)/syscall5.c \
	syscall/$(ARCH)/syscall4.c \
	syscall/$(ARCH)/syscall3.c \
	syscall/$(ARCH)/syscall2.c \
	syscall/$(ARCH)/syscall1.c \
	syscall/$(ARCH)/syscall0.c \
	src/memcpy.c

INCS := syscall libc/include
INCLUDEPATHS := $(realpath $(INCS))
INCLUDEARGS := $(patsubst %,-I%,$(INCLUDEPATHS))

CFLAGS += $(INCLUDEARGS) -O2

include $(BUILDROOT)/library.mk
