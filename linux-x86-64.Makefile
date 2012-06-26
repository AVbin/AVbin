SONAME=libavbin.so.$(AVBIN_VERSION)
LIBNAME=$(OUTDIR)/$(SONAME)

CFLAGS += -fPIC -fno-stack-protector -O3 -m64 --std=gnu89
LDFLAGS += -shared -soname $(SONAME) -Bsymbolic

STATIC_LIBS = -whole-archive \
              $(LIBAV)/libavformat/libavformat.a \
              $(LIBAV)/libavcodec/libavcodec.a \
              $(LIBAV)/libavutil/libavutil.a \
              $(LIBAV)/libswscale/libswscale.a \
              -no-whole-archive \
              -R /usr/local/lib \
              -R .

LIBS = -lbz2

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
