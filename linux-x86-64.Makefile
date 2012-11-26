SONAME=libavbin.so.$(AVBIN_VERSION)
LIBNAME=$(OUTDIR)/$(SONAME)

CFLAGS += -fPIC -O3
LDFLAGS += -shared -soname $(SONAME) -Bsymbolic -zmuldefs

STATIC_LIBS = -whole-archive \
              $(BACKEND_DIR)/libavformat/libavformat.a \
              $(BACKEND_DIR)/libavcodec/libavcodec.a \
              $(BACKEND_DIR)/libavutil/libavutil.a \
              $(BACKEND_DIR)/libswscale/libswscale.a \
              -no-whole-archive

# Unlike the 32-bit, we'll dynamically link libbz2 and hope that distros
# have more consistent library versioning in 64-bit.
LIBS = -lbz2 -lz

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(LD) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
