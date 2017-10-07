isodisk.img: bootiso-bin \
		bootiso.sym mbr.sym kernel.sym diskiso.mk \
		$(top_srcdir)/populate_iso.sh \
		$(top_srcdir)/mkposixdirs.sh
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
