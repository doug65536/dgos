
# Common phony targets
.PHONY: all clean

#
# C, C++, Assembly autodependencies and compilation
#

# Directory to store auto-generated dependency makefile fragments
DEPDIR := .d

# Make sure dependency file directory exists
$(shell mkdir -p $(DEPDIR) >/dev/null)

# Flags to use when generating autodependencies
DEPFLAGS = -MT $@ -MMD -MP -MF $(DEPDIR)/$(notdir $<).d

# Compile commands for C and C++
COMPILE.c = $(CC) $(DEPFLAGS) $(CFLAGS) $(CPPFLAGS) $(TARGET_ARCH)
COMPILE.cc = $(CXX) $(DEPFLAGS) $(CXXFLAGS) $(CPPFLAGS) $(TARGET_ARCH)
COMPILE.s = $(AS) $(ASFLAGS) $(TARGET_ARCH_AS)

OUTPUT_OPTION = -o $@

# Everything depends upon Makefile
$(OBJS): Makefile

# Compile assembly
.s.o:
	$(COMPILE.s) $(OUTPUT_OPTION) \
		-c $<

# Compile C
.c.o:
	$(COMPILE.c) \
		$(OUTPUT_OPTION) -c $<

# Generate assembly dump for C
$(DUMPDIR)/%.s : %.c
$(DUMPDIR)/%.s : %.c $(DEPDIR)/%.d
	$(COMPILE.c) $(OUTPUT_OPTION) -fverbose-asm -S $<

# Compile C++ with cc extension

.cc.o:
	$(COMPILE.cc) $(OUTPUT_OPTION) -c $<

# Compile C++ with cxx extension
.cxx.o:
	$(COMPILE.cc) $(OUTPUT_OPTION) -c $<

# Compile C++ with cpp extension
.cpp.o:
	$(COMPILE.cc) $(OUTPUT_OPTION) -c $<

ifdef DISASSEMBLEFLAGS
# Disassemble
$(DUMPDIR)/%.dis : $(BINDIR)/%.bin
$(DUMPDIR)/%.dis : $(BINDIR)/%.bin
	mkdir -p $(dir $@)
	$(OBJDUMP) $(DISASSEMBLEFLAGS) $< > $@
endif

ifdef DISASSEMBLEELFFLAGS
$(DUMPDIR)/%.disasm : $(BINDIR)/%.bin
$(DUMPDIR)/%.disasm : $(BINDIR)/%.bin
	mkdir -p $(dir $@)
	$(OBJDUMP) $(DISASSEMBLEELFFLAGS) $< > $@
endif

# Hex Dump
$(DUMPDIR)/%.hex : $(BINDIR)/%.bin
$(DUMPDIR)/%.hex : $(BINDIR)/%.bin
	$(HEXDUMP) -C $< > $@

# Tolerate the dependency files being missing
$(DEPDIR)/%.d: ;

# Prevent deletion of generated dependencies
.PRECIOUS: $(DEPDIR)/%.d

# Include the generated makefile fragments
-include $(patsubst %,$(DEPDIR)/%.d,$(notdir $(CSRCS)))
