#!/bin/sh

PREFIX=/usr/local/lib
AVBIN_LIBRARY=libavbin.@AVBIN_VERSION@.dylib

if [ ! -d "$PREFIX" ]
then
  mkdir -p $PREFIX
fi

/bin/ln -sf /usr/local/lib/$AVBIN_LIBRARY $PREFIX/libavbin.dylib
/bin/chmod a+rx $PREFIX/libavbin.dylib
