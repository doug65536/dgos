$(top_builddir)/fatdisk.img: mbr-bin bootfat-bin \
		bootfat.sym mbr.sym kernel.sym diskfat.mk \
		$(top_srcdir)/populate_fat.sh \
		$(top_srcdir)/mkposixdirs.sh
	set -x && \
		rm -f fatpart.img fatdisk.img && \
		truncate --size=261120K fatpart.img && \
		mkfs.vfat -R 128 -F 32 fatpart.img && \
		dd if=bootfat-bin of=fatpart.img \
			bs=1 count=2 conv=notrunc && \
		dd if=bootfat-bin of=fatpart.img \
			bs=1 seek=90 skip=90 \
			count=422 conv=notrunc && \
		dd if=bootfat-bin of=fatpart.img \
			bs=1 seek=1024 skip=1024 \
			conv=notrunc && \
		SRCDIR="$(top_srcdir)" \
			$(top_srcdir)/populate_fat.sh \
				fatpart.img "$(top_srcdir)" && \
		truncate --size=262144K fatdisk.img && \
		echo -e 'o\nn\np\n1\n2048\n\nt\nc\na\np\nw\n' | \
			fdisk fatdisk.img && \
		dd if=fatpart.img of=fatdisk.img \
			bs=512 seek=2048 conv=notrunc && \
		dd if=mbr-bin of=fatdisk.img \
			bs=1 count=446 conv=notrunc && \
		dd if=mbr-bin of=fatdisk.img \
			bs=1 seek=510 skip=510 count=2 conv=notrunc

#		fsck.vfat -v fatpart.img &&

