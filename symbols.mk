OBJDUMP ?= objdump
GREP ?= grep
SED ?= sed
SORT ?= sort

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

kernel-sse4.sym: kernel-sse4 \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP="$(OBJDUMP)" SORT="$(SORT)" \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

kernel-avx2.sym: kernel-avx2 \
		$(top_srcdir)/symbols.mk $(top_srcdir)/gensymtab.sh
	OBJDUMP=$(OBJDUMP) SORT=$(SORT) \
		$(top_srcdir)/gensymtab.sh "$(OBJDUMP)" "$@" "$<"

mbr.dis: mbr-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source -m i8086 $< > $@

bootfat.dis: bootfat-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< > $@

bootiso.dis: bootiso-elf $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< > $@

kernel-generic.dis: kernel-generic $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< > $@

kernel-sse4.dis: kernel-sse4 $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< > $@

kernel-avx2.dis: kernel-avx2 $(top_srcdir)/symbols.mk
	$(OBJDUMP) --disassemble --demangle --source $< > $@
