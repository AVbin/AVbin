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

fail() {
    echo "Fatal: $1"
    exit 1
}

dist_common() {
    echo "Creating distribution for $PLATFORM"
    rm -rf $DIR
    mkdir -p $DIR
    cp $OS.README $DIR/README.txt || fail "Failed copying the README file"
    cp $LIBRARY $DIR/ || fail "Failed copying the library"
    if [ $PLATFORM == "linux-x86-32" -o $PLATFORM == "linux-x86-64" \
            -o $PLATFORM == "macosx-universal" -o $PLATFORM == "macosx-x86-32" \
            -o $PLATFORM == "macosx-x86-64" ]; then
	sed s/@AVBIN_VERSION@/$AVBIN_VERSION/ $OS.install.sh > $DIR/install.sh || fail "Failed creating install.sh"
	chmod a+x $DIR/install.sh || fail "Failed making install.sh executable"
    fi
    pushd dist > /dev/null
    if [ $PLATFORM == "win32" -o $PLATFORM == "win64" ]; then
	7z a -tzip $BASEDIR.zip $BASEDIR || fail "Failed creating zip file - is the 7z binary from the 7zip package insalled?"
    else
	tar cjvf $BASEDIR.tar.bz2 $BASEDIR || fail "Failed creating tarball"
    fi
    popd > /dev/null
    rm -rf $DIR
}

platforms=$*

if [ ! "$platforms" ]; then
    echo "Usage: ./dist.sh <platform> [<platform> [<platform> ...]]"
    echo
    echo "Supported platforms:"
    echo "  linux-x86-32"
    echo "  linux-x86-64"
    echo "  macosx-x86-32"
    echo "  macosx-x86-64"
    echo "  macosx-universal"
    echo "  win32"
    echo "  win64"
    exit 1
fi

for PLATFORM in $platforms; do
    BASEDIR=avbin-$PLATFORM-v$AVBIN_VERSION
    DIR=dist/$BASEDIR
    if [ $PLATFORM == "linux-x86-32" -o $PLATFORM == "linux-x86-64" ]; then
	     OS=linux
	     LIBRARY=dist/$PLATFORM/libavbin.so.$AVBIN_VERSION
    elif [ $PLATFORM == "macosx-universal" -o $PLATFORM == "macosx-x86-32" -o $PLATFORM == "macosx-x86-64" ]; then
	     OS=macosx
	     LIBRARY=dist/$PLATFORM/libavbin.$AVBIN_VERSION.dylib
    elif [ $PLATFORM == "win32" ]; then
	     OS=win32
	     LIBRARY=dist/$PLATFORM/avbin.dll
    elif [ $PLATFORM == "win64" ]; then
	     OS=win64
	     LIBRARY=dist/$PLATFORM/avbin64.dll
    else
        echo "Unsupported platform $PLATFORM"
        exit 1
    fi
    dist_common
done

echo "Done.  File is in dist/"
