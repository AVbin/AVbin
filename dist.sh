#!/bin/bash
#
# dist.sh
# Copyright 2007 Alex Holkner
#
# This file is part of AVbin.
#
# AVbin is free software; you can redistribute it and/or modify it
# under the terms of the GNU Lesser General Public License as
# published by the Free Software Foundation; either version 3 of
# the License, or (at your option) any later version.
#
# AVbin is distributed in the hope that it will be useful, but
# WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Lesser Lesser General Public License for more details.
#
# You should have received a copy of the GNU Lesser General Public
# License along with this program.  If not, see
# <http://www.gnu.org/licenses/>.

AVBIN_VERSION=`cat VERSION`
FFMPEG_REVISION=`cat ffmpeg.revision`

dist_common() {
    rm -rf $1
    mkdir -p $1
}

dist_linux() {
    BASEDIR=avbin-$PLATFORM-$AVBIN_VERSION
    DIR=dist/$BASEDIR
    dist_common $DIR
    cp dist/$PLATFORM/libavbin.so.$AVBIN_VERSION $DIR/
    cp README.linux $DIR/README
    sed s/@AVBIN_VERSION@/$AVBIN_VERSION/ install.sh.linux > $DIR/install.sh
    chmod a+x $DIR/install.sh
    pushd dist
    tar czf $BASEDIR.tar.gz $BASEDIR
    popd
    rm -rf $DIR
}

dist_darwin() {
    BASEDIR=avbin-$PLATFORM-$AVBIN_VERSION
    DIR=dist/$BASEDIR
    dist_common $DIR
    cp README.darwin $DIR/readme.txt
    cp dist/$PLATFORM/libavbin.$AVBIN_VERSION.dylib $DIR/
    sed s/@AVBIN_VERSION@/$AVBIN_VERSION/ install.sh.darwin > $DIR/install.sh
    chmod a+x $DIR/install.sh
    pushd dist
    zip -r $BASEDIR.zip $BASEDIR
    popd
    rm -rf $DIR
}

dist_win32() {
    BASEDIR=avbin-$PLATFORM-$AVBIN_VERSION
    DIR=dist/$BASEDIR
    dist_common $DIR
    cp dist/$PLATFORM/avbin.dll $DIR/
    cp README.win32 $DIR/readme.txt
    pushd dist
    #zip -r $BASEDIR.zip $BASEDIR
    popd
    #rm -rf $DIR
}

dist_source() {
    BASEDIR=avbin-src-$AVBIN_VERSION
    DIR=dist/$BASEDIR
    dist_common $DIR
    cat MANIFEST | xargs -I{} cp {} $DIR
    mkdir -p $DIR/src
    cp src/avbin.c $DIR/src
    mkdir -p $DIR/include
    cp include/avbin.h $DIR/include
    mkdir -p $DIR/doc/html
    cp doc/html/* $DIR/doc/html
    pushd dist
    tar czf $BASEDIR.tar.gz $BASEDIR
    popd
    rm -rf $DIR
}

platforms=$*

if [ ! "$platforms" ]; then
    echo "Usage: ./dist.sh <platform> [<platform> [<platform> ...]]"
    echo
    echo "Supported platforms:"
    echo "  source"
    echo "  linux-x86-32"
    echo "  linux-x86-64"
    echo "  darwin-universal"
    echo "  win32"
    exit 1
fi

for PLATFORM in $platforms; do
    if [ $PLATFORM == "source" ]; then
        dist_source
    elif [ $PLATFORM == "linux-x86-32" -o $PLATFORM == "linux-x86-64" ]; then
        dist_linux
    elif [ $PLATFORM == "darwin-universal" ]; then
        dist_darwin
    elif [ $PLATFORM == "win32" ]; then
        dist_win32
    else
        echo "Unsupported platform $PLATFORM"
        exit 1
    fi
done
