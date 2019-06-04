
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
		$(top_builddir)/initrd \
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
			$(top_builddir)/mbrdisk.img \
			$(top_builddir)/gptdisk.img && \
		\
		truncate --size=48M $(top_builddir)/fatpart.img && \
		\
		mkfs.vfat \
			-F 32 \
			-S $(SECTOR_SZ) \
			-R $$(( 0x10000 / $(SECTOR_SZ) )) \
			-b $$(( 0x10000 / $(SECTOR_SZ) - 1 )) \
			-n DGOS \
			$(top_builddir)/fatpart.img && \
			\
		$(DD) \
			if=$(top_builddir)/boot1-bin \
			of=$(top_builddir)/fatpart.img \
			bs=1 count=$$(( 0xB )) \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/boot1-bin \
			of=$(top_builddir)/fatpart.img \
			bs=1 \
			seek=$$(( 0x5A )) \
			skip=$$(( 0x5A )) \
			count=$$(( 0x1FE - 0x5A )) \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/bootfat-bin \
			of=$(top_builddir)/fatpart.img \
			bs=$(SECTOR_SZ) \
			seek=2 \
			skip=0 \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/fatpart.img \
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

DGOS_UUID=0615151f-d802-4edf-914a-734dc4f03687

$(top_builddir)/mbrdisk.img: \
		fatpart.img
	truncate --size=64M $(top_builddir)/mbrdisk.img && \
		\
		printf 'label: dos\nlength=2048, uuid=%s, name=DGOS, bootable\n' \
				$(DGOS_GUID) | \
			sfdisk $(top_builddir)/mbrdisk.img && \
			\
		$(DD) if=$(top_builddir)/fatpart.img \
			of=$(top_builddir)/mbrdisk.img \
			bs=$(SECTOR_SZ) \
			seek=2048 \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/mbr-bin \
			of=$(top_builddir)/mbrdisk.img \
			bs=1 \
			count=446 \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/mbr-bin \
			of=$(top_builddir)/mbrdisk.img \
			bs=1 seek=510 \
			skip=510 \
			count=2 \
			conv=notrunc

$(top_builddir)/gptdisk.img: fatpart.img
	truncate --size=64M $(top_builddir)/gptdisk.img && \
	\
		printf 'label: gpt\n2048,,U,*' | \
			sfdisk $(top_builddir)/gptdisk.img && \
			\
		sgdisk -A 1:set:2 $(top_builddir)/gptdisk.img && \
		\
		$(DD) if=$(top_builddir)/fatpart.img \
			of=$(top_builddir)/gptdisk.img \
			bs=$(SECTOR_SZ) \
			seek=2048 \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/mbr-bin \
			of=$(top_builddir)/gptdisk.img \
			bs=1 \
			count=446 \
			conv=notrunc && \
			\
		$(DD) if=$(top_builddir)/mbr-bin \
			of=$(top_builddir)/gptdisk.img \
			bs=1 seek=510 \
			skip=510 \
			count=2 \
			conv=notrunc

$(top_builddir)/mbrdisk.qcow: $(top_builddir)/mbrdisk.img
	$(QEMU_IMG) convert -f raw -O qcow -p $< $@

$(top_builddir)/gptdisk.qcow: $(top_builddir)/gptdisk.img
	$(QEMU_IMG) convert -f raw -O qcow -p $< $@
