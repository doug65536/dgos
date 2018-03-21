isodisk.img: bootiso-bin \
		diskiso.mk \
		$(top_srcdir)/populate_iso.sh \
		$(top_srcdir)/mkposixdirs.sh \
		bootiso.sym mbr.sym \
		kernel-generic.dis.gz \
		kernel-bmi.dis.gz \
		bootiso.dis.gz  \
		mbr.dis.gz \
		hello.km
	$(top_srcdir)/populate_iso.sh $(top_srcdir)
	export size=$$(wc -c < bootiso-bin) && \
	export blocks=$$(( (size + 511) / 512 )) && \
	$(MKISOFS) -input-charset utf8 \
		-o $@ \
		-b bootiso-bin \
		-iso-level 2 \
		-no-emul-boot \
		-boot-load-size $$blocks \
		-boot-info-table \
		-r -J \
		-A 'ea870ef2-2483-11e8-9bba-3f1a71a07f83' \
		$(top_builddir)/iso_stage
