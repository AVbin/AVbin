#!/bin/sh

PREFIX=/usr/local/lib
AVBIN_LIBRARY=libavbin.@AVBIN_VERSION@.dylib

touch $PREFIX 2> /dev/null || echo "Insufficient priveleges; run sudo"
touch $PREFIX 2> /dev/null || exit 1

cp $AVBIN_LIBRARY $PREFIX/
ln -sf $AVBIN_LIBRARY $PREFIX/libavbin.dylib
chmod a+rx $PREFIX/$AVBIN_LIBRARY
