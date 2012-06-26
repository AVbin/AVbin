SONAME=libavbin.so.$(AVBIN_VERSION)
LIBNAME=$(OUTDIR)/$(SONAME)

CFLAGS += -fPIC -fno-stack-protector -O3 -m32 --std=gnu99
LDFLAGS += -shared -soname $(SONAME) -melf_i386

STATIC_LIBS = -whole-archive \
              $(LIBAV)/libavformat/libavformat.a \
              $(LIBAV)/libavcodec/libavcodec.a \
              $(LIBAV)/libavutil/libavutil.a \
              $(LIBAV)/libswscale/libswscale.a \
              -no-whole-archive

LIBS = 
#-lbz2

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
