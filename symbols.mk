$(top_builddir)/sym/bootfat.sym: $(top_builddir)/bootfat-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

$(top_builddir)/sym/bootx64-efi.sym: $(top_builddir)/bootefi-amd64 \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" p "$@" "$<"
	$(SED) -i 's/^00000000000/00000000004/g' "$@"

#$(top_builddir)/sym/bootia32-efi.sym: bootfat-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" p "$@" "$<"

$(top_builddir)/sym/bootiso.sym: $(top_builddir)/bootiso-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

$(top_builddir)/sym/mbr.sym: $(top_builddir)/mbr-elf \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

$(top_builddir)/sym/kernel-generic.sym: $(top_builddir)/kernel-generic \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

$(top_builddir)/sym/kernel-tracing.sym: $(top_builddir)/kernel-tracing \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

$(top_builddir)/sym/kernel-asan.sym: $(top_builddir)/kernel-asan \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	$(MKDIR) -p $(@D)
	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"

$(top_builddir)/sym/bootefi-amd64.dis.gz: $(top_builddir)/bootefi-amd64 \
		$(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

$(top_builddir)/sym/mbr.dis.gz: $(top_builddir)/mbr-elf $(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source -m i8086 $< | $(GZIP) > $@

$(top_builddir)/sym/bootfat.dis.gz: $(top_builddir)/bootfat-elf  \
		$(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

$(top_builddir)/sym/bootiso.dis.gz: $(top_builddir)/bootiso-elf  \
		$(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

$(top_builddir)/sym/kernel-generic.dis.gz: $(top_builddir)/kernel-generic  \
		$(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

$(top_builddir)/sym/kernel-tracing.dis.gz: $(top_builddir)/kernel-tracing  \
		$(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@

$(top_builddir)/sym/kernel-asan.dis.gz: $(top_builddir)/kernel-asan  \
		$(top_srcdir)/symbols.mk
	$(MKDIR) -p $(@D)
	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
