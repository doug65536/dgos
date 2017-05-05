QEMU := qemu-system-x86_64

QEMU_MONITOR := \
	-chardev socket,id=qemu-monitor,host=localhost,port=7777,server,nowait,telnet

QEMU_DEBUGCON := \
	-chardev pipe,path=dump/qemu-debug-out,id=qemu-debug-out \
	-mon qemu-monitor,mode=readline \
	-device isa-debugcon,chardev=qemu-debug-out

QEMU_SERIAL := \
	-chardev socket,id=qemu-serial-socket,host=localhost,port=7778,server,nowait \
	-serial chardev:qemu-serial-socket

QEMU_CPU := host,migratable=false,host-cache-info=true
QEMU_RAM := 5G
QEMU_FLAGS :=
QEMU_BRIDGE :=

QEMU_USB := -device nec-usb-xhci \
	-device usb-kbd \
	-device usb-mouse

QEMU_NET := \
	-net nic,model=rtl8139 \
	-net nic,model=ne2k_pci \
	-net nic,model=e1000 \
	$(QEMU_BRIDGE) \
	-net dump,file=dump/netdump

QEMU_COMMON := \
	$(QEMU_MONITOR) \
	$(QEMU_SERIAL) \
	$(QEMU_DEBUGCON) \
	$(QEMU_USB) \
	$(QEMU_NET) \
	-s \
	-no-shutdown -no-reboot -d unimp,guest_errors \
	-m $(QEMU_RAM) \
	$(QEMU_FLAGS)

QEMU_WAIT := -S
QEMU_RUN :=

QEMU_CPUS := 8
QEMU_CORES := 4
QEMU_THREADS := 2
QEMU_SMP := -smp cpus=$(QEMU_CPUS),cores=$(QEMU_CORES),threads=$(QEMU_THREADS)
QEMU_UP := -smp cpus=1,cores=1,threads=1

QEMU_HDCTL_DEV_ahci := -machine q35
QEMU_HDCTL_DEV_ide := -machine pc

# $(call QEMU_STORAGE,image,interface,media)
QEMU_STORAGE = $(QEMU_HDCTL_DEV_$(2)) -drive file=$(1),format=raw,media=$(3)

QEMU_STORAGE_FAT = $(call QEMU_STORAGE,$(DISKIMAGE),$(1),disk)
QEMU_STORAGE_ISO = $(call QEMU_STORAGE,$(ISO_FILE),$(1),cdrom)

QEMU_AHCI := ahci
QEMU_IDE := ide

QEMU_KVM := -enable-kvm -cpu $(QEMU_CPU)
#QEMU_TCG := -cpu kvm64 -accel tcg,thread=multi
QEMU_TCG := -cpu kvm64

QEMU_ISO_DEPS := $(ISO_FILE)
QEMU_FAT_DEPS := debuggable-kernel-disk $(DISKIMAGE)

BOCHS_FLAGS :=

monitor-debug-output:
	while true; do cat dump/qemu-debug-out; done

monitor-connect:
	telnet localhost 7777

.PHONY: monitor-debug-output

# Targets
#  {debug|run}-{up|smp}-{iso|fat}-{ahci|ide}-{kvm|tcg}
#   |   |       |  |     |   |     |    |     |   |
#   |   |       |  |     |   |     |    |     |   QEMU_TCG
#   |   |       |  |     |   |     |    |     QEMU_KVM
#   |   |       |  |     |   |     |    QEMU_IDE
#   |   |       |  |     |   |     QEMU_AHCI
#   |   |       |  |     |   QEMU_FAT
#   |   |       |  |     QEMU_ISO
#   |   |       |  QEMU_SMP
#   |   |       QEMU_UP
#   |   QEMU_RUN
#   QEMU_WAIT

debug-up-iso-ahci-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_TCG)

debug-up-iso-ahci-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_KVM)

debug-up-iso-ide-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_TCG)

debug-up-iso-ide-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_KVM)

debug-up-fat-ahci-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_TCG)

debug-up-fat-ahci-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_KVM)

debug-up-fat-ide-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_TCG)

debug-up-fat-ide-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_KVM)

debug-smp-iso-ahci-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_TCG)

debug-smp-iso-ahci-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_KVM)

debug-smp-iso-ide-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_TCG)

debug-smp-iso-ide-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_KVM)

debug-smp-fat-ahci-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_TCG)

debug-smp-fat-ahci-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_KVM)

debug-smp-fat-ide-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_TCG)

debug-smp-fat-ide-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_WAIT) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_KVM)

run-up-iso-ahci-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_TCG)

run-up-iso-ahci-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_KVM)

run-up-iso-ide-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_TCG)

run-up-iso-ide-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_KVM)

run-up-fat-ahci-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_TCG)

run-up-fat-ahci-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_KVM)

run-up-fat-ide-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_TCG)

run-up-fat-ide-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_UP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_KVM)

run-smp-iso-ahci-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_TCG)

run-smp-iso-ahci-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_AHCI)) \
	$(QEMU_KVM)

run-smp-iso-ide-tcg: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_TCG)

run-smp-iso-ide-kvm: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_ISO,$(QEMU_IDE)) \
	$(QEMU_KVM)

run-smp-fat-ahci-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_TCG)

run-smp-fat-ahci-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_AHCI)) \
	$(QEMU_KVM)

run-smp-fat-ide-tcg: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_TCG)

run-smp-fat-ide-kvm: $(QEMU_FAT_DEPS)
	$(QEMU) $(QEMU_COMMON) \
	$(QEMU_RUN) \
	$(QEMU_SMP) \
	$(call QEMU_STORAGE_FAT,$(QEMU_IDE)) \
	$(QEMU_KVM)

#  {debug|run}-{up|smp}-{iso|fat}-{ahci|ide}-{kvm|tcg}
.PHONY: debug-up-iso-ahci-kvm
.PHONY: debug-up-iso-ahci-tcg
.PHONY: debug-up-iso-ide-kvm
.PHONY: debug-up-iso-ide-tcg
.PHONY: debug-up-fat-ahci-kvm
.PHONY: debug-up-fat-ahci-tcg
.PHONY: debug-up-fat-ide-kvm
.PHONY: debug-up-fat-ide-tcg
.PHONY: debug-smp-iso-ahci-kvm
.PHONY: debug-smp-iso-ahci-tcg
.PHONY: debug-smp-iso-ide-kvm
.PHONY: debug-smp-iso-ide-tcg
.PHONY: debug-smp-fat-ahci-kvm
.PHONY: debug-smp-fat-ahci-tcg
.PHONY: debug-smp-fat-ide-kvm
.PHONY: debug-smp-fat-ide-tcg
.PHONY: run-up-iso-ahci-kvm
.PHONY: run-up-iso-ahci-tcg
.PHONY: run-up-iso-ide-kvm
.PHONY: run-up-iso-ide-tcg
.PHONY: run-up-fat-ahci-kvm
.PHONY: run-up-fat-ahci-tcg
.PHONY: run-up-fat-ide-kvm
.PHONY: run-up-fat-ide-tcg
.PHONY: run-smp-iso-ahci-kvm
.PHONY: run-smp-iso-ahci-tcg
.PHONY: run-smp-iso-ide-kvm
.PHONY: run-smp-iso-ide-tcg
.PHONY: run-smp-fat-ahci-kvm
.PHONY: run-smp-fat-ahci-tcg
.PHONY: run-smp-fat-ide-kvm
.PHONY: run-smp-fat-ide-tcg

# ---

debug-iso-wait-many-ahci: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
		$(QEMU_TCG) \
		$(QEMU_SMP) \
		$(QEMU_ISO) \
		-device ich9-ahci,id=ahci2 \
		-device ich9-ahci,id=ahci3 \
		-device ich9-ahci,id=ahci4

debug-iso-bridge-test-wait: $(QEMU_ISO_DEPS)
	$(QEMU) $(QEMU_COMMON) \
		$(QEMU_TCG) \
		$(QEMU_SMP) \
		$(QEMU_ISO) \
		-device i82801b11-bridge,bus=pcie.0,id=pcie.1 \
		-device i82801b11-bridge,bus=pcie.1,id=pcie.2 \
		-device i82801b11-bridge,bus=pcie.2,id=pcie.3 \
		-device i82801b11-bridge,bus=pcie.3,id=pcie.4 \
		-device i82801b11-bridge,bus=pcie.4,id=pcie.5 \
		-device i82801b11-bridge,bus=pcie.5,id=pcie.6 \
		-device ich9-ahci,bus=pcie.1,id=ahci2 \
		-device ich9-ahci,bus=pcie.2,id=ahci3 \
		-device ich9-ahci,bus=pcie.2,id=ahci4 \
		-device ich9-ahci,bus=pcie.4,id=ahci5 \
		-device ich9-ahci,bus=pcie.5,id=ahci6 \
		-device ich9-ahci,bus=pcie.6,id=ahci7 \
		-device ich9-ahci,bus=pcie.6,id=ahci8

#
# Get GDB

debug-iso-boot: $(QEMU_ISO_DEPS)
	$(GDB) $(SYMBOLFILE) --tui \
		-ex 'b elf64_run' \
		-ex 'target remote | exec $(QEMU) $(QEMU_COMMON) \
			$(QEMU_TCG) \
			$(QEMU_ISO) \
			-gdb stdio'

debug-iso-kernel: $(QEMU_ISO_DEPS)
	$(GDB) --symbols $(KERNELSYMBOLFILE) --tui \
		-ex 'b entry' \
		-ex 'target remote | exec $(QEMU) $(QEMU_COMMON) \
			$(QEMU_TCG) \
			-accel tcg,thread=multi \
			$(QEMU_ISO) \
			-gdb stdio'

#
# Debug in KVM

$(BINDIR)/debug-kvm-gdbcommands: utils/debug-kvm-commands.template
	$(SED) "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

$(BINDIR)/debug-kernel-kvm-gdbcommands: utils/debug-kernel-kvm-commands.template
	$(SED) "s|\$$SYMBOLFILE|$(KERNELSYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

#		-netdev user,id=mynet0,net=192.168.66.0/24,dhcpstart=192.168.66.9 \
#		-net nic,model=i82551 \
#		-net nic,model=i82557b \
#		-net nic,model=i82559er \
#		-net nic,model=pcnet \
#		-net nic,model=smc91c111 \
#		-net nic,model=lance \
#		-net nic,model=ne2k_isa \
#		-net nic,model=mcf_fec

#
# Debug in bochs

bochs-symbols: $(BOCHSCOMBINEDSYMBOLS)

$(BOCHSCOMBINEDSYMBOLS): $(BOCHSKERNELSYMBOLS) $(BOCHSSYMBOLS)
	$(CAT) $^ > $@

$(BOCHSKERNELSYMBOLS): kernel
	$(OBJDUMP) --wide --syms ../kernel/bin/kernel | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

$(BOCHSSYMBOLS): $(SYMBOLFILE)
	$(OBJDUMP) --wide --syms $^ | \
		$(GREP) -P '^[0-9A-Fa-f_]+\s.*\s[a-zA-Z_][a-zA-Z0-9_]+$$' | \
		$(SED) -r 's/^(\S+)\s+.*\s+(\S+)$$/\1 \2/' | \
		$(SORT) > $@

$(BINDIR)/bochs-iso-config.bxrc: utils/bochs-iso-config.bxrc.template
	$(SED) "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

$(BINDIR)/bochs-fat-config.bxrc: utils/bochs-fat-config.bxrc.template
	$(SED) "s|\$$SYMBOLFILE|$(SYMBOLFILE)|g" $< | \
		$(SED) "s|\$$DISKIMAGE|$(DISKIMAGE)|g" > $@

debug-iso-bochs: all bochs-symbols iso $(BINDIR)/bochs-iso-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-iso-config.bxrc \
		-rc utils/bochs-debugger-commands \
		$(BOCHS_FLAGS)

debug-fat-bochs: all bochs-symbols debuggable-kernel-disk $(BINDIR)/bochs-fat-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-fat-config.bxrc \
		-rc utils/bochs-debugger-commands \
		$(BOCHS_FLAGS)

debug-iso-bochs-boot: all bochs-symbols iso $(BINDIR)/bochs-iso-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-iso-config.bxrc \
		-rc utils/bochs-debugger-boot-commands \
		$(BOCHS_FLAGS)

debug-fat-bochs-boot: all bochs-symbols debuggable-kernel-disk $(BINDIR)/bochs-fat-config.bxrc
	$(BOCHS) \
		-qf $(BINDIR)/bochs-fat-config.bxrc \
		-rc utils/bochs-debugger-boot-commands \
		$(BOCHS_FLAGS)

run-iso-bochs: all bochs-symbols iso $(BINDIR)/bochs-iso-config.bxrc
	$(BOCHS) -qf $(BINDIR)/bochs-iso-config.bxrc -q \
		$(BOCHS_FLAGS)

run-fat-bochs: all bochs-symbols debuggable-kernel-disk $(BINDIR)/bochs-fat-config.bxrc
	$(BOCHS) -qf $(BINDIR)/bochs-fat-config.bxrc -q \
		$(BOCHS_FLAGS)
