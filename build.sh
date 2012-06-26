#!/bin/bash
#
# build.sh
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
FFMPEG_REVISION=`cat ffmpeg.revision`

# Directory holding ffmpeg source code.
FFMPEG=libav

if git rev-parse > /dev/null 2> /dev/null ; then
    if ! ls libav/Makefile >/dev/null 2> /dev/null ; then
        git submodule init --quiet
        git submodule update
    fi
fi

fail() {
    echo "AVbin: Fatal error: $1"
    exit 1
}

clean_libav() {
    pushd $FFMPEG > /dev/null
    echo -n "Cleaning up..."
    make clean 2> /dev/null
    make distclean 2> /dev/null
    echo "done"
    find . -name '*.d' -exec rm -f '{}' ';'
    find . -name '*.pc' -exec rm -f '{}' ';'
    rm -f config.log config.err config.h config.mak .config .version
    popd > /dev/null
}

build_ffmpeg() {
    config=`pwd`/ffmpeg.configure.$PLATFORM
    common=`pwd`/ffmpeg.configure.common

    if [ ! $REBUILD ]; then
	clean_libav
    fi

    pushd $FFMPEG > /dev/null

    # If we're not rebuilding, then we need to configure FFmpeg
    if [ ! $REBUILD ]; then
	case $OSX_VERSION in
            "10.6") SDKPATH="\/Developer\/SDKs\/MacOSX10.6.sdk" ;;
            "10.7") SDKPATH="\/Applications\/Xcode.app\/Contents\/Developer\/Platforms\/MacOSX.platform\/Developer\/SDKs\/MacOSX10.6.sdk" ;;
            *)      SDKPATH="" ;;
	esac

        cat $config $common | egrep -v '^#' | sed s/%%SDKPATH%%/$SDKPATH/g | xargs ./configure || fail "Failed configuring libav."

	# Patch the generated config.h file if a patch for this build exists
	if [ -e ../patch.config.h-${PLATFORM} ] ; then
	    echo "AVbin: Found custom config.h patch for this architecture."
	    patch -p0 < ../patch.config.h-${PLATFORM} || fail "Failed applying config.h patch"
	    echo "AVbin: Patch succeeded."
	fi
    fi

    # Remove -Werror options from config.mak that break builds on some platforms
    cat config.mak | sed -e s/-Werror=implicit-function-declaration//g | sed -e s/-Werror=missing-prototypes//g > config.mak2
    mv config.mak2 config.mak

    # Actually build FFmpeg
    make -j3 || fail "Failed to build libav."
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
    make || fail "Failed to build AVbin."
}

build_darwin_universal() {
    if [ ! -e dist/darwin-x86-32/libavbin.$AVBIN_VERSION.dylib ]; then
        PLATFORM=darwin-x86-32
        build_ffmpeg
        build_avbin
    fi

    if [ ! -e dist/darwin-x86-64/libavbin.$AVBIN_VERSION.dylib ]; then
        PLATFORM=darwin-x86-64
        build_ffmpeg
        build_avbin
    fi

    mkdir -p dist/darwin-universal
    lipo -create \
        -output dist/darwin-universal/libavbin.$AVBIN_VERSION.dylib \
        dist/darwin-x86-32/libavbin.$AVBIN_VERSION.dylib \
        dist/darwin-x86-64/libavbin.$AVBIN_VERSION.dylib || fail "Failed to create universal shared library."
}

die_usage() {
    echo "Usage: ./build.sh [options] <platform> [<platform> [<platform> ...]]"
    echo
    echo "Options"
    echo "  --clean     Don't build, just clean up all generated files and directories."
    echo "  --help      Display this help text."
    echo "  --rebuild   Don't reconfigure, just run make again."
    echo
    echo "Supported platforms:"
    echo "  linux-x86-32"
    echo "  linux-x86-64"
    echo "  darwin-x86-32"
    echo "  darwin-x86-64"
    echo "  darwin-universal (builds all supported darwin architectures into one library)"
    echo "  win32"
    echo "  win64"
    exit 1
}

while [ "${1:0:2}" == "--" ]; do
    case $1 in
        "--help")
            die_usage ;;
        "--rebuild")
            REBUILD=1;;
        "--clean")
	    clean_libav
            rm -rf dist
            rm -rf build
            exit
            ;;
        *)
            echo "Unrecognised option: $1.  Try Try ./build.sh --help" && exit 1
            ;;
    esac
    shift
done;

platforms=$*

if [ ! "$platforms" ]; then
    die_usage
fi

for PLATFORM in $platforms; do
    case $PLATFORM in
        "darwin-universal")
            OSX_VERSION=`/usr/bin/sw_vers -productVersion | cut -b 1-4`
            build_darwin_universal
            ;;
        "darwin-x86-32" | "darwin-x86-64")
            OSX_VERSION=`/usr/bin/sw_vers -productVersion | cut -b 1-4`
            build_ffmpeg
            build_avbin
            ;;
        "linux-x86-32" | "linux-x86-64" | "win32" | "win64")
            build_ffmpeg
            build_avbin
            ;;
        *)
            echo "Unrecognized platform: $PLATFORM.  Try ./build.sh --help" && exit 3
            ;;
    esac
done

echo "Build successful."
