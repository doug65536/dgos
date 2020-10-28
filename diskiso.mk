
#bootia32.efi

isodisk.iso: \
		$(top_srcdir)/mkposixdirs.bash \
		$(top_srcdir)/diskiso.mk \
		$(top_srcdir)/populate_iso.bash
	$(top_srcdir)/populate_iso.bash $(top_srcdir)
	size=$$(wc -c < bootiso-bin) \
		&& \
		blocks=$$(( ( (size + 2047) / 2048) * 4 )) \
		&& \
		if ((blocks > 8)); then blocks=8; fi \
		&& \
		size=$$(wc -c < fatpart.img ) \
		&& \
		fatblocks=$$(( ( (size + 2047) / 2048) * 4 )) \
		&& \
		pxebootfile="boot/bootiso-bin" \
		&& \
		efibootfile="EFI/boot/fatpart.img" \
		&& \
		$(TOUCH) -r "iso_stage/boot/bootiso-bin" \
			"bootiso-bin-timestamp-backup" \
		&& \
		$(MKISOFS) -input-charset utf8 \
			-o "$@" \
			-eltorito-alt-boot \
			-b "$$pxebootfile" \
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
