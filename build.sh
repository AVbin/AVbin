#!/bin/bash
#
# build.sh
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

# Directory holding ffmpeg checkout.
FFMPEG=ffmpeg

patch_ffmpeg() {
    patch -d $FFMPEG -p0 < $1
}

build_ffmpeg() {
    if [ ! -d $FFMPEG ]; then
        echo "$FFMPEG does not exist, please symlink to your local checkout."
        exit 1
    fi

    config=`pwd`/ffmpeg.configure.$PLATFORM
    common=`pwd`/ffmpeg.configure.common

    pushd $FFMPEG
    if [ ! $REBUILD ]; then
        make distclean
        cat $config $common | egrep -v '^#' | xargs ./configure || exit 1
    fi
    make || exit 1
    popd
}

build_avbin() {
    export AVBIN_VERSION
    export FFMPEG_REVISION
    export PLATFORM
    export FFMPEG
    if [ ! $REBUILD ]; then
        make clean
    fi
    make || exit 1
}

build_darwin_universal() {
    if [ ! -e dist/darwin-x86-32/libavbin.$AVBIN_VERSION.dylib ]; then
        PLATFORM=darwin-x86-32
        build_ffmpeg
        build_avbin
    fi

    if [ ! -e dist/darwin-ppc-32/libavbin.$AVBIN_VERSION.dylib ]; then
        PLATFORM=darwin-ppc-32
        build_ffmpeg
        build_avbin
    fi

    mkdir -p dist/darwin-universal
    lipo -create \
        -output dist/darwin-universal/libavbin.$AVBIN_VERSION.dylib \
        dist/darwin-x86-32/libavbin.$AVBIN_VERSION.dylib \
        dist/darwin-ppc-32/libavbin.$AVBIN_VERSION.dylib
}

while [ "${1:0:2}" == "--" ]; do
    case $1 in
        "--help") # fall through
            ;;
        "--rebuild") REBUILD=1;;
        "--patch") 
            shift
            if [ ! "$1" -o ! -f "$1" ]; then
                echo "No patch file specified or file does not exist."
                exit 1
            fi
            patch_ffmpeg $1
            exit
            ;;
        *)           echo "Unrecognised option: $1" && exit 1;;
    esac
    shift
done;

platforms=$*

if [ ! "$platforms" ]; then
    echo "Usage: ./build.sh [options] <platform> [<platform> [<platform> ...]]"
    echo "   or: ./build.sh --patch <patchfile>"
    echo
    echo "Options"
    echo "  --rebuild           Don't reconfigure, just run make."
    echo "  --patch <file>      Apply a patch to ffmpeg."
    echo
    echo "Supported platforms:"
    echo "  linux-x86-32"
    echo "  linux-x86-64"
    echo "  darwin-ppc-32"
    echo "  darwin-x86-32"
    echo "  darwin-x86-64"
    echo "  darwin-universal (32-bit only)"
    echo "  win32"
    echo "  win64"
    exit 1
fi

for PLATFORM in $platforms; do
    if [ $PLATFORM == "darwin-universal" ]; then
        build_darwin_universal
    else
        build_ffmpeg
        build_avbin
    fi
done
