BUILDROOT := $(realpath ../kernel)
export BUILDROOT

MODULES := hello

all: modules

modules: $(MODULES)

$(MODULES):
	(cd $@ && $(MAKE))

.PHONY: all modules $(MODULES)
