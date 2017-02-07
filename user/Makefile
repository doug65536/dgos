BUILDROOT := $(realpath ../kernel)
export BUILDROOT

all: libc

clean:
	(cd libc && $(MAKE))

libc:
	(cd libc && $(MAKE))

.PHONY: all clean
