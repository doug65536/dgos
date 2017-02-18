BUILDROOT := $(realpath ../kernel)
export BUILDROOT

all: libc shell

clean:
	(cd libc && $(MAKE) clean)
	(cd shell && $(MAKE) clean)

libc:
	(cd libc && $(MAKE))

shell:
	(cd shell && $(MAKE))

.PHONY: all clean libc shell
