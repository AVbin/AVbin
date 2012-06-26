LIBNAME=$(OUTDIR)/avbin64.dll

CC = x86_64-w64-mingw32-gcc

CFLAGS += -O3
LDFLAGS += -shared

STATIC_LIBS = -Wl,-whole-archive \
              -Wl,$(LIBAV)/libavformat/libavformat.a \
              -Wl,$(LIBAV)/libavcodec/libavcodec.a \
              -Wl,$(LIBAV)/libavutil/libavutil.a \
              -Wl,$(LIBAV)/libswscale/libswscale.a \
              -Wl,-no-whole-archive

LIBS = -lm -lws2_32

$(LIBNAME) : $(OBJNAME) $(OUTDIR)
	$(CC) $(LDFLAGS) -o $@ $< $(STATIC_LIBS) $(LIBS)
