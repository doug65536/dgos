
#bootia32.efi

isodisk.iso: \
		$(top_srcdir)/mkposixdirs.bash \
		$(top_srcdir)/diskiso.mk \
		$(top_srcdir)/populate_iso.bash
	$(top_srcdir)/populate_iso.bash $(top_srcdir)
	size=$$(wc -c < bootiso-bin) \
		&& \
		sectorsize=2048 \
		&& \
		loadbase=0x1000 \
		&& \
		loadend=0x8000 \
		&& \
		loadsz=$$(( loadend - loadbase )) \
		&& \
		blocks=$$(( (size + sectorsize - 1) / sectorsize)) \
		&& \
		if (( blocks > loadsz / sectorsize )); \
			then blocks=$$(( loadsz / sectorsize )); \
		fi \
		&& \
		size=$$(wc -c < fatpart.img ) \
		&& \
		bootfile="boot/bootiso-bin" \
		&& \
		efibootfile="EFI/boot/fatpart.img" \
		&& \
		$(TOUCH) -r "iso_stage/boot/bootiso-bin" \
			"bootiso-bin-timestamp-backup" \
		&& \
		$(MKISOFS) -input-charset utf8 \
			-o "$@" \
			-eltorito-alt-boot \
			-b "$$bootfile" \
			-iso-level 2 \
			-no-emul-boot \
			-boot-load-size "$$blocks" \
			-boot-info-table \
			-boot-load-seg 0x100 \
			-r -J -f \
			-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
			-eltorito-alt-boot \
			-b "$$efibootfile" \
			-iso-level 2 \
			-no-emul-boot \
			-r -J -f \
			-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
			"iso_stage/" \
		&& \
		$(TOUCH) -r "bootiso-bin-timestamp-backup" \
			"iso_stage/boot/bootiso-bin" \
		&& \
		$(RM) -f "bootiso-bin-timestamp-backup"
