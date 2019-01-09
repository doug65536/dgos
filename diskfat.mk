
#bootia32.efi

# Stitch byte 0-0xA of boot1-bin into offset 0 of partition
# Stitch byte 0x5A-0x1FD of boot1-bin into offset 0x5A of partition
#  (Avoids overwriting BPB)
# Copy the entire bootfat-bin into sector 2 of the partition
#  (Avoids overwriting FS information sector)

$(top_builddir)/fatpart.img: \
		$(top_srcdir)/diskfat.mk \
		\
		$(top_builddir)/dgos-kernel-generic \
		$(top_builddir)/kernel-generic \
		$(top_builddir)/kernel-generic.sym \
		$(top_builddir)/kernel-generic.dis.gz \
		\
		$(top_builddir)/dgos-kernel-tracing \
		$(top_builddir)/kernel-tracing \
		$(top_builddir)/kernel-tracing.sym \
		$(top_builddir)/kernel-tracing.dis.gz \
		\
		$(top_builddir)/dgos-kernel-asan \
		$(top_builddir)/kernel-asan \
		$(top_builddir)/kernel-asan.sym \
		$(top_builddir)/kernel-asan.dis.gz \
		\
		$(top_builddir)/bootx64.efi \
		$(top_builddir)/bootx64-efi.sym \
		$(top_builddir)/bootefi-amd64.dis.gz \
		\
		$(top_builddir)/bootpxe-bios-elf \
		$(top_builddir)/bootpxe-bios-bin \
		$(top_builddir)/bootpxe-bios.map \
		\
		$(top_builddir)/user-shell \
		$(top_builddir)/hello.km \
		\
		$(top_srcdir)/mkposixdirs.sh \
		\
		$(top_srcdir)/diskfat.mk \
		$(top_srcdir)/populate_fat.sh \
		\
		$(top_builddir)/mbr-bin \
		$(top_builddir)/mbr.sym \
		$(top_builddir)/mbr.dis.gz \
		\
		$(top_builddir)/boot1-bin \
		$(top_builddir)/boot1-elf \
		\
		$(top_builddir)/bootfat-bin \
		$(top_builddir)/bootfat.sym \
		$(top_builddir)/bootfat.dis.gz
	set -x && \
		rm -f $(top_builddir)/fatpart.img \
			$(top_builddir)/fatdisk.img && \
		\
		truncate --size=80M $(top_builddir)/fatpart.img && \
		\
		mkfs.vfat \
			-F 32 \
			-S $(SECTOR_SZ) \
			-R $$(( 0x10000 / $(SECTOR_SZ) )) \
			-b $$(( 0x10000 / $(SECTOR_SZ) - 1 )) \
			-n dgos \
			$(top_builddir)/fatpart.img && \
			\
		dd \
			if=$(top_builddir)/boot1-bin \
			of=$(top_builddir)/fatpart.img \
			bs=1 count=$$(( 0xB )) \
			conv=notrunc && \
			\
		dd if=$(top_builddir)/boot1-bin \
			of=$(top_builddir)/fatpart.img \
			bs=1 \
			seek=$$(( 0x5A )) \
			skip=$$(( 0x5A )) \
			count=$$(( 0x1FE - 0x5A )) \
			conv=notrunc && \
			\
		dd if=$(top_builddir)/bootfat-bin \
			of=$(top_builddir)/fatpart.img \
			bs=$(SECTOR_SZ) \
			seek=2 \
			skip=0 \
			conv=notrunc && \
			\
		dd if=$(top_builddir)/fatpart.img \
			of=$(top_builddir)/fatpart.img \
			bs=$(SECTOR_SZ) \
			count=1 \
			skip=0 \
			seek=$$(( 0x10000 / $(SECTOR_SZ) - 1 )) \
			conv=notrunc && \
			\
		SRCDIR="$(top_srcdir)" \
			$(top_srcdir)/populate_fat.sh \
				$(top_builddir)/fatpart.img "$(top_srcdir)"

$(top_builddir)/fatdisk.img: \
		$(top_srcdir)/diskfat.mk \
		\
		fatpart.img \
		\
		$(top_builddir)/mbr-bin \
		$(top_builddir)/mbr.sym \
		$(top_builddir)/mbr.dis.gz \
		\
		$(top_builddir)/boot1-bin \
		$(top_builddir)/boot1-elf \
		\
		$(top_builddir)/bootfat-bin \
		$(top_builddir)/bootfat.sym \
		$(top_builddir)/bootfat.dis.gz
	truncate --size=262144K fatdisk.img && \
		\
		echo -e 'o\nn\np\n1\n2048\n\nt\nc\na\np\ni\nw\n' | \
			fdisk -b $(SECTOR_SZ) $(top_builddir)/fatdisk.img && \
			\
		dd if=$(top_builddir)/fatpart.img \
			of=$(top_builddir)/fatdisk.img \
			bs=$(SECTOR_SZ) \
			seek=2048 \
			conv=notrunc && \
			\
		dd if=$(top_builddir)/mbr-bin \
			of=$(top_builddir)/fatdisk.img \
			bs=1 \
			count=446 \
			conv=notrunc && \
			\
		dd if=$(top_builddir)/mbr-bin \
			of=$(top_builddir)/fatdisk.img \
			bs=1 seek=510 \
			skip=510 \
			count=2 \
			conv=notrunc

$(top_builddir)/fatdisk.qcow: $(top_builddir)/fatdisk.img
	$(QEMU_IMG) convert -f raw -O qcow -p $< $@
