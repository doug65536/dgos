
EXTERNDIR := extern

EXTERNINCLUDEDIR := $(EXTERNDIR)/include
ZLIBINCLUDEDIR := $(EXTERNINCLUDEDIR)/zlib
ZLIBHEADERS := zlib.h zconf.h

ZLIBVER := 1.2.11
ZLIBBUILDROOT := $(EXTERNDIR)/zlib-src
ZLIBSOURCEDIR := $(ZLIBBUILDROOT)/zlib-$(ZLIBVER)
ZLIBGZFILE := zlib-$(ZLIBVER).tar.gz
ZLIBURL := http://zlib.net/$(ZLIBGZFILE)
ZLIBCFLAGS := -mcmodel=kernel -mno-red-zone -msse -msse2 -fno-exceptions -fno-common -O2 -fbuiltin
ZLIBCONFIGURE := $(ZLIBSOURCEDIR)/configure
ZLIBLIBDIR := $(EXTERNDIR)/zlib-src/build
ZLIBLIBNAME := libz.a
ZLIBSTATICLIB := $(ZLIBLIBDIR)/$(ZLIBLIBNAME)
ZLIBCONFIGUREFLAGS := --64 --static
ZLIBMAKEFILE := $(ZLIBLIBDIR)/Makefile
ZLIBMAKE := $(MAKE)

ZLIBHEADERSOURCES := $(patsubst %,$(ZLIBSOURCEDIR)/%,$(ZLIBHEADERS))
ZLIBPUBLICINCLUDES := $(patsubst %,$(ZLIBINCLUDEDIR)/%,$(ZLIBHEADERS))

# Make zlib
$(ZLIBSTATICLIB) $(ZLIBPUBLICINCLUDES): $(ZLIBMAKEFILE)
	( cd $(@D) && $(ZLIBMAKE) $(ZLIBLIBNAME) )
	mkdir -p $(ZLIBINCLUDEDIR)
	for i in $(ZLIBHEADERS); do \
		cp $(ZLIBSOURCEDIR)/$$i $(ZLIBINCLUDEDIR)/$$i;\
	done

# Run zlib configure
$(ZLIBMAKEFILE): $(ZLIBCONFIGURE)
	( \
		mkdir -p $(@D) && \
		cd $(@D) && \
		CFLAGS="$(ZLIBCFLAGS)" ../zlib-$(ZLIBVER)/configure $(ZLIBCONFIGUREFLAGS) \
	)

# Download zlib
$(ZLIBCONFIGURE):
	mkdir -p $(ZLIBBUILDROOT)
	curl -o $(ZLIBBUILDROOT)/zlib.tar.gz $(ZLIBURL)
	(cd $(ZLIBBUILDROOT) && tar -xzvf zlib.tar.gz)
