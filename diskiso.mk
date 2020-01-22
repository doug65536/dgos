
#bootia32.efi

$(top_builddir)/isodisk.img: \
		$(top_builddir)/kernel-generic \
		\
		$(top_builddir)/kernel-tracing \
		\
		$(top_builddir)/kernel-asan \
		\
		$(top_builddir)/bootx64.efi \
		\
		$(top_builddir)/bootpxe-bios-elf \
		$(top_builddir)/bootpxe-bios-bin \
		$(top_builddir)/bootpxe-bios.map \
		\
		$(top_builddir)/initrd \
		\
		$(top_srcdir)/mkposixdirs.sh \
		\
		$(top_srcdir)/diskiso.mk \
		$(top_srcdir)/populate_iso.sh \
		\
		$(top_builddir)/fatpart.img \
		\
		$(top_builddir)/iso_stage \
		$(shell test [ "$(top_builddir)/iso_stage" ] && \
			$(FIND) -L "$(top_builddir)/iso_stage" || true) \
		\
		$(top_builddir)/bootiso-bin
	$(top_srcdir)/populate_iso.sh $(top_srcdir)
	size=$$(wc -c < bootiso-bin) && \
	blocks=$$(( ( (size + 2047) / 2048) * 4 )) && \
	if ((blocks > 8)); then blocks=8; fi && \
	size=$$(wc -c < fatpart.img ) && \
	fatblocks=$$(( ( (size + 2047) / 2048) * 4 )) && \
	$(MKISOFS) -input-charset utf8 \
		-o "$@" \
		-eltorito-alt-boot \
		-b "bootiso-bin" \
		-iso-level 2 \
		-no-emul-boot \
		-boot-load-size "$$blocks" \
		-boot-info-table \
		-boot-load-seg 0x7c0 \
		-r -J \
		-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
		-eltorito-alt-boot \
		-b efipart.img \
		-iso-level 2 \
		-no-emul-boot \
		-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
		"$(top_builddir)/iso_stage"
