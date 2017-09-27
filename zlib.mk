EXTERNDIR = $(abs_top_builddir)/extern
ZLIBVER = 1.2.11
ZLIBBUILDROOT = $(EXTERNDIR)/zlib-src
ZLIBSOURCEDIR = $(ZLIBBUILDROOT)/zlib-$(ZLIBVER)
ZLIBGZFILE = zlib-$(ZLIBVER).tar.gz
ZLIBURL = http://zlib.net/$(ZLIBGZFILE)
ZLIBCFLAGS = -O3 -mcmodel=kernel -fno-pie \
	-mno-red-zone -msse -msse2 -fno-exceptions \
	-fno-common -fno-stack-protector -fbuiltin \
	-fno-asynchronous-unwind-tables

EXTERNINCLUDEDIR = $(EXTERNDIR)/include
ZLIBINCLUDEDIR = $(EXTERNINCLUDEDIR)/zlib
ZLIBHEADERS = zlib.h zconf.h

ZLIBHEADERSOURCES = $(ZLIBSOURCEDIR)/zlib.h $(ZLIBSOURCEDIR)/zconf.h
ZLIBPUBLICINCLUDES = $(ZLIBINCLUDEDIR)/zlib.h $(ZLIBINCLUDEDIR)/zconf.h

ZLIBCONFIGURE = $(ZLIBSOURCEDIR)/configure
ZLIBBUILDDIR = $(EXTERNDIR)/zlib-src/build
ZLIBLIBNAME = libz.a
ZLIBSTATICLIB = $(ZLIBBUILDDIR)/$(ZLIBLIBNAME)
ZLIBCONFIGUREFLAGS = --64 --static
ZLIBMAKEFILE = $(ZLIBBUILDDIR)/Makefile
ZLIBMAKE = $(MAKE)

# Make zlib
$(ZLIBSTATICLIB) $(ZLIBPUBLICINCLUDES) $(ZLIBHEADERSOURCES):
	echo top build dir $(abs_top_builddir)
	echo Downloading zlib
	mkdir -p $(ZLIBBUILDROOT)
	curl -o $(ZLIBBUILDROOT)/zlib.tar.gz $(ZLIBURL)
	echo Extracting zlib
	(cd $(ZLIBBUILDROOT) && tar -xzvf zlib.tar.gz)
	echo Building zlib
	mkdir -p $(@D)
	mkdir -p $(ZLIBINCLUDEDIR)
	+( \
		( \
			cd $(@D) && \
			echo Configuring zlib && \
			CFLAGS="$(ZLIBCFLAGS)" CXX=$(CXX) CC=$(CC) $(ZLIBBUILDROOT)/zlib-$(ZLIBVER)/configure $(ZLIBCONFIGUREFLAGS) && \
			echo Invoking $(ZLIBMAKE) $(ZLIBLIBNAME) && \
			$(ZLIBMAKE) $(ZLIBLIBNAME) \
		) && \
		mkdir -p $(ZLIBINCLUDEDIR) && \
		for i in $(ZLIBHEADERS); do \
			cp $(ZLIBSOURCEDIR)/$$i $(ZLIBINCLUDEDIR)/$$i;\
		done \
	)
