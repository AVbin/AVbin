SONAME=libavbin.so.$(AVBIN_VERSION)
LIBNAME=$(OUTDIR)/$(SONAME)

CFLAGS += -fPIC -fno-stack-protector -O3 -m64 --std=gnu89
LDFLAGS += -shared -soname $(SONAME) -Bsymbolic -zmuldefs

STATIC_LIBS = -whole-archive \
              $(BACKEND_DIR)/libavformat/libavformat.a \
              $(BACKEND_DIR)/libavcodec/libavcodec.a \
              $(BACKEND_DIR)/libavutil/libavutil.a \
              $(BACKEND_DIR)/libswscale/libswscale.a \
              -no-whole-archive \
              -R /usr/local/lib \
              -R .

LIBS = -lbz2

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
