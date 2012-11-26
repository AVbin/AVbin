SONAME=libavbin.so.$(AVBIN_VERSION)
LIBNAME=$(OUTDIR)/$(SONAME)

CFLAGS += -fPIC -O3
LDFLAGS += -shared -soname $(SONAME) -zmuldefs

STATIC_LIBS = -whole-archive \
              $(BACKEND_DIR)/libavformat/libavformat.a \
              $(BACKEND_DIR)/libavcodec/libavcodec.a \
              $(BACKEND_DIR)/libavutil/libavutil.a \
              $(BACKEND_DIR)/libswscale/libswscale.a \
              -no-whole-archive

# Statically link libbz2 since different distros name the library differently
LIBS = -Bstatic -lbz2 -Bdynamic -lz

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
