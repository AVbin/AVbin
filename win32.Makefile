LIBNAME=$(OUTDIR)/avbin.dll

CC = i686-w64-mingw32-gcc

CFLAGS += -O3
LDFLAGS += -shared -Wl,-allow-multiple-definition

STATIC_LIBS = -Wl,-whole-archive \
              -Wl,$(BACKEND_DIR)/libavformat/libavformat.a \
              -Wl,$(BACKEND_DIR)/libavcodec/libavcodec.a \
              -Wl,$(BACKEND_DIR)/libavutil/libavutil.a \
              -Wl,$(BACKEND_DIR)/libswscale/libswscale.a \
              -Wl,-no-whole-archive

LIBS = -lbz2 -lz

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
