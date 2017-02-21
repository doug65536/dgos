BUILDROOT := $(realpath ../../kernel)
export BUILDROOT

MODNAME := hello

CSRCS := hello.c

ASRCS :=

CFLAGS := -mcmodel=kernel -g \
	-std=c11

LDFLAGS := -g

include $(BUILDROOT)/module.mk
