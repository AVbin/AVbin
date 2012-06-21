#!/bin/bash
#
# dist.sh
# Copyright 2012 AVbin Team
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

dist_common() {
    echo "Creating distribution for $PLATFORM"
    rm -rf $DIR
    mkdir -p $DIR
}

dist_linux() {
    dist_common
    cp dist/$PLATFORM/libavbin.so.$AVBIN_VERSION $DIR/
    cp README.linux $DIR/README
    sed s/@AVBIN_VERSION@/$AVBIN_VERSION/ install.sh.linux > $DIR/install.sh
    chmod a+x $DIR/install.sh
    pushd dist > /dev/null
    tar cjf $BASEDIR.tar.bz2 $BASEDIR
    popd > /dev/null
    rm -rf $DIR
}

dist_darwin() {
    dist_common
    cp README.darwin $DIR/readme.txt
    cp dist/$PLATFORM/libavbin.$AVBIN_VERSION.dylib $DIR/
    sed s/@AVBIN_VERSION@/$AVBIN_VERSION/ install.sh.darwin > $DIR/install.sh
    chmod a+x $DIR/install.sh
    pushd dist
    tar cjf $BASEDIR.tar.bz2 $BASEDIR
    popd
    rm -rf $DIR
}

dist_win32() {
    dist_common
    cp dist/$PLATFORM/avbin.dll $DIR/
    cp README.win32 $DIR/readme.txt
    pushd dist
    7z a -tzip $BASEDIR.zip $BASEDIR
    popd
    rm -rf $DIR
}

dist_win64() {
    dist_common
    cp dist/$PLATFORM/avbin64.dll $DIR/
    cp README.win64 $DIR/readme.txt
    pushd dist
    7z a -tzip $BASEDIR.zip $BASEDIR
    popd
    rm -rf $DIR
}

dist_source() {
    dist_common
    rmdir $DIR
    echo "Creating source archive from current commit."
    git archive --prefix=$BASEDIR/ HEAD | bzip2 -9 > dist/$BASEDIR.tar.bz2
}

platforms=$*

if [ ! "$platforms" ]; then
    echo "Usage: ./dist.sh <platform> [<platform> [<platform> ...]]"
    echo
    echo "Supported platforms:"
    echo "  source"
    echo "  linux-x86-32"
    echo "  linux-x86-64"
    echo "  darwin-x86-32"
    echo "  darwin-x86-64"
    echo "  darwin-universal"
    echo "  win32"
    echo "  win64"
    exit 1
fi

for PLATFORM in $platforms; do
    BASEDIR=avbin-$PLATFORM-v$AVBIN_VERSION
    DIR=dist/$BASEDIR
    if [ $PLATFORM == "source" ]; then
        dist_source
    elif [ $PLATFORM == "linux-x86-32" -o $PLATFORM == "linux-x86-64" ]; then
        dist_linux
    elif [ $PLATFORM == "darwin-universal" -o $PLATFORM == "darwin-x86-32" -o $PLATFORM == "darwin-x86-64" ]; then
        dist_darwin
    elif [ $PLATFORM == "win32" ]; then
        dist_win32
    elif [ $PLATFORM == "win64" ]; then
        dist_win64
    else
        echo "Unsupported platform $PLATFORM"
        exit 1
    fi
done

echo "Done.  File is in dist/"