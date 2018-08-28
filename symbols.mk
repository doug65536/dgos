bootfat.sym: bootfat-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

bootx64-efi.sym: bootefi-amd64 \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" p "$@" "$<"
	$(SED) -i 's/^00000000000/00000000004/g' "$@"

#bootia32-efi.sym: bootfat-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" p "$@" "$<"

bootiso.sym: bootiso-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

mbr.sym: mbr-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

kernel-generic.sym: kernel-generic \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

kernel-tracing.sym: kernel-tracing \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

kernel-asan.sym: kernel-asan \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

bootefi-amd64.dis.gz: bootefi-amd64 $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@ &

mbr.dis.gz: mbr-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source -m i8086 $< | $(GZIP) > $@ &

bootfat.dis.gz: bootfat-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@ &

bootiso.dis.gz: bootiso-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@ &

kernel-generic.dis.gz: kernel-generic $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@ &

kernel-tracing.dis.gz: kernel-tracing $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@ &

kernel-asan.dis.gz: kernel-asan $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@ &
