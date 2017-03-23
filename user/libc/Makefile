
BUILDROOT ?= $(realpath ../../kernel)

ARCH ?= x86_64

LIBNAME := c
CSRCS := \
	syscall/$(ARCH)/syscall6.cc \
	syscall/$(ARCH)/syscall5.cc \
	syscall/$(ARCH)/syscall4.cc \
	syscall/$(ARCH)/syscall3.cc \
	syscall/$(ARCH)/syscall2.cc \
	syscall/$(ARCH)/syscall1.cc \
	syscall/$(ARCH)/syscall0.cc \
	src/memcpy.cc

INCS := syscall libc/include
INCLUDEPATHS := $(realpath $(INCS))
INCLUDEARGS := $(patsubst %,-I%,$(INCLUDEPATHS))

CXXFLAGS += $(INCLUDEARGS) -O2

include $(BUILDROOT)/library.mk
