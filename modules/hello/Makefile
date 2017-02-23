BUILDROOT := $(realpath ../../kernel)
export BUILDROOT

MODNAME := hello

CSRCS := hello.c

ASRCS :=

CFLAGS := -mcmodel=kernel -g \
	-std=c11 -fno-common -fno-exceptions

LDFLAGS := -g

include $(BUILDROOT)/module.mk
