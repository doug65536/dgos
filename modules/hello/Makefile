BUILDROOT := $(realpath ../../kernel)
export BUILDROOT

MODNAME := hello

CSRCS := hello.cc

ASRCS :=

CXXFLAGS := -g -mcmodel=kernel -fno-pie \
	-std=c++14 -fno-common -fno-exceptions

#-fno-asynchronous-unwind-tables

LDFLAGS := -g -r

include $(BUILDROOT)/module.mk
