bootfat.sym: bootfat-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

bootiso.sym: bootiso-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

mbr.sym: mbr-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

kernel-generic.sym: kernel-generic \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

kernel-bmi.sym: kernel-bmi \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

mbr.dis.gz: mbr-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source -m i8086 $< | $(GZIP) > $@

bootfat.dis.gz: bootfat-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

bootiso.dis.gz: bootiso-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

kernel-generic.dis.gz: kernel-generic $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

kernel-bmi.dis.gz: kernel-bmi $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
