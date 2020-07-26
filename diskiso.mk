
#bootia32.efi

isodisk.iso: \
		bootiso-bin \
		fatpart.img \
		\
		$(top_srcdir)/mkposixdirs.sh \
		$(top_srcdir)/diskiso.mk \
		$(top_srcdir)/populate_iso.sh \
		\
		fatpart.img
	$(top_srcdir)/populate_iso.sh $(top_srcdir)
	size=$$(wc -c < bootiso-bin) && \
		blocks=$$(( ( (size + 2047) / 2048) * 4 )) && \
		if ((blocks > 8)); then blocks=8; fi && \
		size=$$(wc -c < fatpart.img ) && \
		fatblocks=$$(( ( (size + 2047) / 2048) * 4 )) && \
		$(MKISOFS) -input-charset utf8 \
			-o "$@" \
			-eltorito-alt-boot \
			-b "boot/bootiso-bin" \
			-iso-level 2 \
			-no-emul-boot \
			-boot-load-size "$$blocks" \
			-boot-info-table \
			-boot-load-seg 0x7c0 \
			-r -J -f \
			-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
			-eltorito-alt-boot \
			-b EFI/boot/fatpart.img \
			-iso-level 2 \
			-no-emul-boot \
			-r -J -f \
			-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
			"iso_stage/"
