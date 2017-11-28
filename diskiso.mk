isodisk.img: bootiso-bin \
		diskiso.mk \
		$(top_srcdir)/populate_iso.sh \
		$(top_srcdir)/mkposixdirs.sh \
		bootiso.sym mbr.sym \
		kernel-generic.dis \
		kernel-sse4.dis \
		kernel-avx2.dis \
		bootiso.dis  \
		mbr.dis
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
		$(top_builddir)/iso_stage
