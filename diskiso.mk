
#bootia32.efi

$(top_builddir)/isodisk.img: \
		kernel-generic \
		kernel-generic.sym \
		kernel-generic.dis.gz \
		\
		kernel-bmi \
		kernel-bmi.sym \
		kernel-bmi.dis.gz \
		\
		bootx64.efi \
		bootx64-efi.sym \
		bootefi-amd64.dis.gz \
		\
		user-shell \
		hello.km \
		\
		$(top_srcdir)/mkposixdirs.sh \
		\
		diskiso.mk \
		$(top_srcdir)/populate_iso.sh \
		\
		bootiso-bin \
		bootiso.sym \
		bootiso.dis.gz
	$(top_srcdir)/populate_iso.sh $(top_srcdir)
	size=$$(wc -c < bootiso-bin) && \
	blocks=$$(( ( (size + 2047) / 2048) * 4 )) && \
	if ((blocks > 8)); then blocks=8; fi && \
	$(MKISOFS) -input-charset utf8 \
		-o "$@" \
		-b "bootiso-bin" \
		-iso-level 2 \
		-no-emul-boot \
		-boot-load-size "$$blocks" \
		-boot-info-table \
		-boot-load-seg 0x7c0 \
		-r -J \
		-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
		"$(top_builddir)/iso_stage"
