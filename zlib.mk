EXTERNDIR = $(top_builddir)/extern
ZLIBVER = 1.2.11
ZLIBBUILDROOT = $(EXTERNDIR)/zlib-src
ZLIBSOURCEDIR = $(ZLIBBUILDROOT)/zlib-$(ZLIBVER)
ZLIBGZFILE = zlib-$(ZLIBVER).tar.gz
ZLIBURL = http://zlib.net/$(ZLIBGZFILE)
ZLIBCFLAGS = -O3 -mcmodel=kernel -fno-pie \
	-mno-red-zone -msse -msse2 -fno-exceptions \
	-fno-common -fno-stack-protector -fbuiltin

EXTERNINCLUDEDIR = $(EXTERNDIR)/include
ZLIBINCLUDEDIR = $(EXTERNINCLUDEDIR)/zlib
ZLIBHEADERS = zlib.h zconf.h

#ZLIBHEADERSOURCES = $(patsubst %,$(ZLIBSOURCEDIR)/%,$(ZLIBHEADERS))
ZLIBHEADERSOURCES = $(ZLIBSOURCEDIR)/zlib.h $(ZLIBSOURCEDIR)/zconf.h

#ZLIBPUBLICINCLUDES = $(patsubst %,$(ZLIBINCLUDEDIR)/%,$(ZLIBHEADERS))
ZLIBPUBLICINCLUDES = $(ZLIBINCLUDEDIR)/zlib.h $(ZLIBINCLUDEDIR)/zconf.h

# -fno-asynchronous-unwind-tables

ZLIBCONFIGURE = $(ZLIBSOURCEDIR)/configure
ZLIBLIBDIR = $(EXTERNDIR)/zlib-src/build
ZLIBLIBNAME = libz.a
ZLIBSTATICLIB = $(ZLIBLIBDIR)/$(ZLIBLIBNAME)
ZLIBCONFIGUREFLAGS = --64 --static
ZLIBMAKEFILE = $(ZLIBLIBDIR)/Makefile
ZLIBMAKE = $(MAKE)

$(ZLIBINCLUDEDIR):
	mkdir -p $(ZLIBINCLUDEDIR)

# Make zlib
$(ZLIBSTATICLIB) $(ZLIBPUBLICINCLUDES) $(ZLIBHEADERSOURCES): $(ZLIBMAKEFILE) $(ZLIBINCLUDEDIR)
	+( \
		( cd $(@D) && $(ZLIBMAKE) $(ZLIBLIBNAME) ); \
		for i in $(ZLIBHEADERS); do \
			cp $(ZLIBSOURCEDIR)/$$i $(ZLIBINCLUDEDIR)/$$i;\
		done \
	)

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

