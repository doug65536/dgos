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

kernel-generic.sym: kernel-generic symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

kernel-sse4.sym: kernel-sse4 symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

kernel-avx2.sym: kernel-avx2 symbols.mk
	$(OBJDUMP) --wide --syms $< | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

mbr.dis: mbr-elf
	objdump --disassemble --source -m i8086 $^ > $@

bootfat.dis: bootfat-elf
	objdump --disassemble --source $^ > $@

bootiso.dis: bootiso-elf
	objdump --disassemble --source $^ > $@

kernel-generic.dis: kernel-generic
	objdump --disassemble --source $^ > $@

kernel-sse4.dis: kernel-sse4
	objdump --disassemble --source $^ > $@

kernel-avx2.dis: kernel-avx2
	objdump --disassemble --source $^ > $@
