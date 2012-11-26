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
AVBIN_PRERELEASE=`cat PRERELEASE 2>/dev/null`
AVBIN_VERSION_STRING="$AVBIN_VERSION$AVBIN_PRERELEASE"

fail() {
    echo "Fatal: $1"
    exit 1
}

dist_common() {
    echo "Creating distribution for $PLATFORM"
    # Clean up, just in case
    rm -rf $DIR
    mkdir -p $DIR
    cp $LIBRARY $DIR/ || fail "Failed copying the library"
    # Get the license
    cp COPYING.LESSER $DIR || fail "Failed copying the license"
    # Get the install script
    if [ $PLATFORM == "linux-x86-32" -o $PLATFORM == "linux-x86-64" \
         -o $PLATFORM == "macosx-x86-32" -o $PLATFORM == "macosx-x86-64" \
         -o $PLATFORM == "macosx-universal" ]; then
        sed s/@AVBIN_VERSION@/$AVBIN_VERSION/ $OS.install.sh \
	    | sed s/@AVBIN_VERSION_STRING@/$AVBIN_VERSION_STRING/ \
	    > $DIR/install.sh \
            || fail "Failed creating install.sh"
        chmod a+x $DIR/install.sh || fail "Failed making install.sh executable"
    fi
    # Create tarball or zipfile
    pushd dist > /dev/null
    if [ $PLATFORM == "win32" -o $PLATFORM == "win64" ]; then
        cat ../avbin.nsi | sed s/@AVBIN_VERSION@/$AVBIN_VERSION_STRING/g \
            | sed s/@AVBIN_LIB_FILENAME@/$AVBIN_LIB_FILENAME/g \
            | sed s/@ARCH@/$ARCH/g > avbin.nsi \
            || fail "Failed creating NSIS configuration file."
	     makensis avbin.nsi || fail "Failed compiling windows installer."
	     echo "Created AVbin${AVBIN_VERSION_STRING}-${ARCH}.exe"
    elif [ $PLATFORM == "linux-x86-64" -o $PLATFORM == "linux-x86-32" ]; then
        # Create binary installer for Linux
	     makeself $BASEDIR install-$BASEDIR "AVbin $AVBIN_VERSION_STRING" \
	         "./install.sh" \
		 || fail "Failed creating the Linux installer."
    fi
    popd > /dev/null
    # Create a binary package installer for OS X
    if [ $PLATFORM == "macosx-universal" ] ; then
        PACKAGE=dist/AVbin${AVBIN_VERSION_STRING}.pkg
        echo -n "Creating $PACKAGE ... "
        /Applications/PackageMaker.app/Contents/MacOS/PackageMaker \
            --doc avbin.pmdoc --out $PACKAGE && echo "done."
    fi
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
    BASEDIR=avbin-$PLATFORM-v$AVBIN_VERSION_STRING
    DIR=dist/$BASEDIR
    if [ $PLATFORM == "linux-x86-32" -o $PLATFORM == "linux-x86-64" ]; then
	     OS=linux
	     LIBRARY=dist/$PLATFORM/libavbin.so.$AVBIN_VERSION
    elif [ $PLATFORM == "macosx-universal" \
           -o $PLATFORM == "macosx-x86-32" \
           -o $PLATFORM == "macosx-x86-64" ]; then
	     OS=macosx
	     LIBRARY=dist/$PLATFORM/libavbin.$AVBIN_VERSION.dylib
    elif [ $PLATFORM == "win32" ]; then
	ARCH=32
	OS=win32
	AVBIN_LIB_FILENAME=avbin.dll
	LIBRARY=dist/$PLATFORM/$AVBIN_LIB_FILENAME
    elif [ $PLATFORM == "win64" ]; then
	ARCH=64
	OS=win64
	AVBIN_LIB_FILENAME=avbin64.dll
	LIBRARY=dist/$PLATFORM/$AVBIN_LIB_FILENAME
    else
        echo "Unsupported platform $PLATFORM"
        exit 1
    fi
    dist_common
done

echo "Done.  File is in dist/"
