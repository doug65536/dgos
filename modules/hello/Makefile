BUILDROOT := $(realpath ../../kernel)
export BUILDROOT

MODNAME := hello

CSRCS := hello.c

ASRCS :=

CFLAGS := -g -mcmodel=kernel \
	-std=c11 -fno-common -fno-exceptions

#-fno-asynchronous-unwind-tables

LDFLAGS := -g -r

include $(BUILDROOT)/module.mk
