#bootia32.efi

# Stitch byte 0-0xA of boot1-bin into offset 0 of partition
# Stitch byte 0x5A-0x1C0 of boot1-bin into offset 0x5A of partition
#  (Avoids overwriting BPB)
# Copy the entire bootfat-bin into sector 2 of the partition
#  (Avoids overwriting FS information sector)

DISK_SIZE_MB=256

# 0x10000 (64KB) reserved for boot code
FATPART_RESERVED_SIZE=0x100000

fatpart.img: \
		mbr-bin \
		boot1-bin \
		bootfat-bin \
		$(top_srcdir)/mkposixdirs.bash \
		$(top_srcdir)/diskfat.mk \
		$(top_srcdir)/populate_fat.bash
	printf "Truncating fatpart.img\n" \
		&& \
		$(TRUNCATE) --size=0 "$@" \
		&& \
		$(TRUNCATE) --size="$(DISK_SIZE_MB)M" "$@" \
		&& \
		printf "Formatting partition\n" \
		&& \
		$(MKFS_VFAT) \
			-F 32 \
			-S "$(SECTOR_SZ)" \
			-R "$$(( $(FATPART_RESERVED_SIZE) / $(SECTOR_SZ) ))" \
			-b "$$(( $(FATPART_RESERVED_SIZE) / $(SECTOR_SZ) - 1 ))" \
			-n DGOS \
			"$@" \
		&& \
		printf "Stitching short jump into partition boot sector\n" \
		&& \
		$(DD) \
			if=boot1-bin \
			of="$@" \
			bs=1 \
			count=$$(( 0x3 )) \
			conv=notrunc \
		&& \
		printf "Stitching 0x5A-0x1FE into partition boot sector\n" \
		&& \
		$(DD) \
			if=boot1-bin \
			of="$@" \
			bs=1 \
			seek="$$(( 0x5A ))" \
			skip="$$(( 0x5A ))" \
			count="$$(( 0x1FE - 0x5A ))" \
			conv=notrunc \
		&& \
		printf "Stitching signature (stolen from mbr)\n" \
		&& \
		$(DD) \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			seek="$$(( 0x1FE ))" \
			skip="$$(( 0x1FE ))" \
			count=2 \
			conv=notrunc \
		&& \
		printf "Installing stage 2\n" \
		&& \
		$(DD) \
			if=bootfat-bin \
			of="$@" \
			bs="$(SECTOR_SZ)" \
			seek=2 \
			skip=0 \
			conv=notrunc \
		&& \
		printf "Make a backup copy of the MBR at the end of reserved area\n" \
		&& \
		$(DD) \
			if="$@" \
			of="$@" \
			bs="$(SECTOR_SZ)" \
			count=1 \
			skip=0 \
			seek="$$(( $(FATPART_RESERVED_SIZE) / $(SECTOR_SZ) - 1 ))" \
			conv=notrunc \
		&& \
		printf "Populating fat partition with files and directories\n" \
		&& \
		SRCDIR="$(top_srcdir)" \
			"$(top_srcdir)/populate_fat.bash" fatpart.img "$(top_srcdir)" && \
		$(TOUCH) fatpart.img

DGOS_MBRID=0x0615151f
DGOS_UUID=0615151f-d802-4edf-914a-734dc4f03687
MBR_PART_LBA = 2048

mbrdisk.img: \
		fatpart.img \
		mbr-bin \
		boot1-bin \
		bootfat-bin
	printf "Truncating disk image\n" \
		&& \
		"$(TRUNCATE)" --size=0 "$@" \
		&& \
		"$(TRUNCATE)" --size="$$(( $(DISK_SIZE_MB) + 2 ))M" "$@" \
		&& \
		printf "Creating partition table\n" \
		&& \
		printf 'label: dos\nlabel: dos\nlabel-id: %s\n$(MBR_PART_LBA),,U,*' \
				"$(DGOS_MBRID)" | \
			"$(SFDISK)" --no-tell-kernel "$@" \
		&& \
		printf "Stitching MBR code into MBR (preserve partition table)\n" \
		&& \
		"$(DD)" \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			count=446 \
			conv=notrunc \
		&& \
		printf "Copying MBR signature\n" \
		&& \
		"$(DD)" \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			seek=510 \
			skip=510 \
			count=2 \
			conv=notrunc \
		&& \
		printf "Copying partition image into disk image\n" \
		&& \
		"$(DD)" \
			if="$<" \
			of="$@" \
			bs="$(SECTOR_SZ)" \
			seek="$$(( $(MBR_PART_LBA) ))" \
			conv=notrunc

HYB_PART_LBA = $(MBR_PART_LBA)

hybdisk.img: \
		fatpart.img \
		mbr-bin \
		boot1-bin \
		bootfat-bin
	printf "Truncating hybrid disk image\n" \
		&& \
		"$(TRUNCATE)" --size=0 "$@" \
		&& \
		"$(TRUNCATE)" --size=$$(( $(DISK_SIZE_MB) + 2))M "$@" \
		&& \
		printf "Creating MBR partition table\n" \
		&& \
		printf 'label: gpt\n$(HYB_PART_LBA),,U,*' | $(SFDISK) "$@" \
		&& \
		printf "Copying boot code into MBR\n" \
		&& \
		"$(DD)" \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			count=446 \
			conv=notrunc \
		&& \
		printf "Copying MBR signature\n" \
		&& \
		"$(DD)" \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			seek=510 \
			skip=510 \
			count=2 \
			conv=notrunc \
		&& \
		printf "Converting MBR to hybrid GPT protective MBR\n" \
		&& \
		"$(SGDISK)" -A 1:set:2 -h 1 "$@" \
		&& \
		printf "Copying partition into disk image\n" \
		&& \
		"$(DD)" \
			if="$<" \
			of="$@" \
			bs=$(SECTOR_SZ) \
			seek=$(HYB_PART_LBA) \
			conv=notrunc

GPT_PART_LBA=$(MBR_PART_LBA)

gptdisk.img: \
		fatpart.img \
		mbr-bin \
		boot1-bin \
		bootfat-bin
	printf "Truncating GPT disk image\n" \
		&& \
		"$(TRUNCATE)" --size=0 "$@" \
		&& \
		"$(TRUNCATE)" --size="$$(( $(DISK_SIZE_MB) + 2))M" "$@" \
		&& \
		printf "Creating GPT partition table\n" \
		&& \
		"$(SGDISK)" -Z -n 1:$(GPT_PART_LBA):0 -t 1:ef00 -A 1:set:2 "$@" \
		&& \
		printf "Copying boot code into MBR\n" \
		&& \
		"$(DD)" \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			count=446 \
			conv=notrunc \
		&& \
		printf "Copying MBR signature\n" \
		&& \
		"$(DD)" \
			if=mbr-bin \
			of="$@" \
			bs=1 \
			seek=510 \
			skip=510 \
			count=2 \
			conv=notrunc \
		&& \
		printf "Copying partition into disk image\n" \
		&& \
		"$(DD)" \
			if="$<" \
			of="$@" \
			bs=$(SECTOR_SZ) \
			seek=$(MBR_PART_LBA) \
			conv=notrunc

mbrdisk.qcow: mbrdisk.img
	$(QEMU_IMG) convert -f raw -O qcow -p $< "$@"

gptdisk.qcow: gptdisk.img
	$(QEMU_IMG) convert -f raw -O qcow -p $< "$@"

gptdisk.qcow.img: gptdisk.qcow
	$(QEMU_IMG) convert -f qcow -O raw -p $< "$@"
