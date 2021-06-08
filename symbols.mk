#sym/bootfat.sym: bootfat-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" e "$@" "$<"
#
#sym/bootx64.sym: bootefi \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" p "$@" "$<"
#	$(SED) -i 's/^00000000000/00000000004/g' "$@"
#
##sym/bootia32-efi.sym: bootfat-elf \
##		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
##	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
##		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" p "$@" "$<"
#
#sym/bootiso.sym: bootiso-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" e "$@" "$<"
#
#sym/mbr.sym: mbr-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" e "$@" "$<"
#
#sym/kernel-generic.sym: kernel-generic \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" e "$@" "$<"
#
#sym/kernel-tracing.sym: kernel-tracing \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" e "$@" "$<"
#
#sym/kernel-asan.sym: kernel-asan \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.bash
#	$(MKDIR) -p $(@D)
#	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
#		$(top_srcdir)/gensymtab.bash "$(OBJDUMP)" e "$@" "$<"
#
#sym/bootefi.dis.gz: bootefi \
#		$(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
#
#sym/mbr.dis.gz: mbr-elf $(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source -m i8086 $< | $(GZIP) > $@
#
#sym/bootfat.dis.gz: bootfat-elf  \
#		$(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
#
#sym/bootiso.dis.gz: bootiso-elf  \
#		$(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
#
#sym/kernel-generic.dis.gz: kernel-generic  \
#		$(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
#
#sym/kernel-tracing.dis.gz: kernel-tracing  \
#		$(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
#
#sym/kernel-asan.dis.gz: kernel-asan  \
#		$(top_srcdir)/symbols.mk
#	$(MKDIR) -p $(@D)
#	$(OBJDUMP) --disassemble --demangle --source $< | $(GZIP) > $@
