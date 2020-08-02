#sym/bootfat.sym: bootfat-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"
#
#sym/bootx64-efi.sym: bootefi-amd64 \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" p "$@" "$<"
#	$(SED) -i 's/^00000000000/00000000004/g' "$@"
#
##sym/bootia32-efi.sym: bootfat-elf \
##		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
##	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
##		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" p "$@" "$<"
#
#sym/bootiso.sym: bootiso-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"
#
#sym/mbr.sym: mbr-elf \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"
#
#sym/kernel-generic.sym: kernel-generic \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"
#
#sym/kernel-tracing.sym: kernel-tracing \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"
#
#sym/kernel-asan.sym: kernel-asan \
#		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
#	$(MKDIR) -p $(@D)
#	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
#		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" e "$@" "$<"
#
#sym/bootefi-amd64.dis.gz: bootefi-amd64 \
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
