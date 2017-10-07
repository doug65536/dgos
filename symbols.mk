OBJDUMP ?= objdump
GREP ?= grep
SED ?= sed
SORT ?= sort

bootfat.sym: bootfat-elf symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

bootiso.sym: bootiso-elf symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

mbr.sym: mbr-elf symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

kernel.sym: kernel-elf symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@
