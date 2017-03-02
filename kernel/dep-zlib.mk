
EXTERNDIR := extern

ZLIBVER := 1.2.11
ZLIBBUILDROOT := $(EXTERNDIR)/zlib-src
ZLIBGZFILE := zlib-$(ZLIBVER).tar.gz
ZLIBURL := http://zlib.net/$(ZLIBGZFILE)
ZLIBCFLAGS := -mcmodel=kernel -mno-red-zone -msse -msse2 -fno-exceptions -fno-common -ffreestanding -O2 -fbuiltin
ZLIBCONFIGURE := $(ZLIBBUILDROOT)/zlib-$(ZLIBVER)/configure
ZLIBLIBDIR := $(EXTERNDIR)/zlib-src/build
ZLIBSTATICLIB := $(ZLIBLIBDIR)/libz.a
ZLIBCONFIGUREFLAGS := --64 --static
ZLIBMAKE := $(MAKE)

$(ZLIBSTATICLIB): $(ZLIBCONFIGURE)
	( \
		mkdir -p $(@D) && \
		cd $(@D) && \
		CFLAGS="$(ZLIBCFLAGS)" ../zlib-$(ZLIBVER)/configure $(ZLIBCONFIGUREFLAGS) && \
		$(ZLIBMAKE) \
	)

# Download zlib
$(ZLIBCONFIGURE):
	mkdir -p $(ZLIBBUILDROOT)
	curl -o $(ZLIBBUILDROOT)/zlib.tar.gz $(ZLIBURL)
	(cd $(ZLIBBUILDROOT) && tar -xzvf zlib.tar.gz)
