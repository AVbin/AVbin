# $Id$

CFLAGS += -DAVBIN_VERSION=$(AVBIN_VERSION) \
          -DFFMPEG_REVISION=$(FFMPEG_REVISION)

CC = gcc
LD = ld
BUILDDIR = build
OUTDIR = dist/$(PLATFORM)

OBJNAME = $(BUILDDIR)/avbin.o

INCLUDE_DIRS = -I include \
               -I $(FFMPEG)

include Makefile.$(PLATFORM)

all : $(LIBNAME)
	ln -sf $(LIBNAME) $(LINKNAME)

$(OBJNAME) : src/avbin.c include/avbin.h $(BUILDDIR)
	$(CC) -c $(CFLAGS) $(INCLUDE_DIRS) -o $@ $<

$(BUILDDIR) :
	mkdir -p $(BUILDDIR)

$(OUTDIR) :
	mkdir -p $(OUTDIR)

clean : 
	rm -f $(OBJNAME)
	rm -f $(LINKNAME)
	rm -f $(LIBNAME)
