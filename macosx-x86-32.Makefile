LIBNAME=$(OUTDIR)/libavbin.$(AVBIN_VERSION).dylib
DARWIN_VERSION=$(shell uname -r | cut -d . -f 1)

CFLAGS += -O3 -mmacosx-version-min=10.6 -arch i386
LDFLAGS += -dylib \
           -single_module \
           -arch i386 \
           -install_name @rpath/libavbin.dylib \
           -macosx_version_min 10.6 \
           -framework CoreFoundation \
           -framework CoreVideo \
           -framework VideoDecodeAcceleration \
           -read_only_relocs suppress

STATIC_LIBS = $(BACKEND_DIR)/libavformat/libavformat.a \
              $(BACKEND_DIR)/libavcodec/libavcodec.a \
              $(BACKEND_DIR)/libavutil/libavutil.a \
              $(BACKEND_DIR)/libswscale/libswscale.a

LIBS = -lSystem \
       -lz \
       -lbz2 \
       -L/usr/lib/gcc/i686-apple-darwin$(DARWIN_VERSION)/4.2.1 \
       -lgcc

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
