BUILDROOT := $(realpath ../kernel)
export BUILDROOT

MODULES := hello

all: modules

clean:
	(for i in $(MODULES); do cd $$i && $(MAKE) clean; done)

modules: $(MODULES)

$(MODULES):
	(cd $@ && $(MAKE))

.PHONY: all clean modules $(MODULES)
