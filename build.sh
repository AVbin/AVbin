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

# Either "libav" or "ffmpeg"
BACKEND=libav

# Directory holding backend source code.
BACKEND_DIR=$BACKEND

# Make sure the git submodule is initiated and updated (if blank)
if ! ls $BACKEND_DIR/Makefile >/dev/null 2> /dev/null ; then
    git submodule init --quiet
    git submodule update
fi


AVBIN_VERSION=`cat VERSION`
AVBIN_PRERELEASE=`cat PRERELEASE 2>/dev/null`
AVBIN_VERSION_STRING="\"$AVBIN_VERSION$AVBIN_PRERELEASE\""
AVBIN_BUILD_DATE="\"`date +\"%Y-%m-%d %H:%M:%S %z\"`\""
AVBIN_REPO="\"`git remote show -n origin | grep Fetch | cut -b 14-`\""
AVBIN_COMMIT="\"`git rev-parse HEAD`\""

# Backend information
AVBIN_BACKEND="\"$BACKEND\""
pushd $BACKEND_DIR > /dev/null
AVBIN_BACKEND_VERSION_STRING="\"`git describe --abbrev=0 --tags`\""
if [ $AVBIN_BACKEND_VERSION_STRING == "\"v0.8\"" ] ; then
    TRUNK_DATE=`git log -1 --pretty=format:%cd`
    AVBIN_BACKEND_VERSION_STRING="\"trunk ($TRUNK_DATE)\""
fi
AVBIN_BACKEND_REPO="\"`git remote show -n origin | grep Fetch | cut -b 14-`\""
AVBIN_BACKEND_COMMIT="\"`git rev-parse HEAD`\""
popd > /dev/null

fail() {
    echo "AVbin: Fatal error: $1"
    exit 1
}

clean_backend() {
    pushd $BACKEND_DIR > /dev/null
    echo -n "Cleaning up..."
    make clean 2> /dev/null
    make distclean 2> /dev/null
    echo "done"
    find . -name '*.d' -exec rm -f '{}' ';'
    find . -name '*.pc' -exec rm -f '{}' ';'
    rm -f config.log config.err config.h config.mak .config .version
    popd > /dev/null
}

build_backend() {
    config=`pwd`/$PLATFORM.configure
    common=`pwd`/common.configure

    if [ ! $REBUILD ]; then
        clean_backend
    fi

    pushd $BACKEND_DIR > /dev/null

    # If we're not rebuilding, then we need to configure Backend
    if [ ! $REBUILD ]; then
        case $OSX_VERSION in
            "10.6") SDKPATH="\/Developer\/SDKs\/MacOSX10.6.sdk" ;;
            "10.7") SDKPATH="\/Applications\/Xcode.app\/Contents\/Developer\/Platforms\/MacOSX.platform\/Developer\/SDKs\/MacOSX10.6.sdk" ;;
            *)      SDKPATH="" ;;
        esac

        cat $config $common | egrep -v '^#' | sed s/%%SDKPATH%%/$SDKPATH/g | xargs ./configure || fail "Failed configuring backend."
    fi

    # Remove -Werror options from config.mak that break builds on some platforms
    cat config.mak | sed -e s/-Werror=implicit-function-declaration//g | sed -e s/-Werror=missing-prototypes//g > config.mak2
    mv config.mak2 config.mak

    # Actually build Backend
    make $FAST || fail "Failed to build backend."
    popd
}

build_avbin() {
    # For avbin.c ...
    export AVBIN_VERSION
    export AVBIN_VERSION_STRING
    export AVBIN_BUILD_DATE
    export AVBIN_REPO
    export AVBIN_COMMIT
    export AVBIN_BACKEND
    export AVBIN_BACKEND_VERSION_STRING
    export AVBIN_BACKEND_REPO
    export AVBIN_BACKEND_COMMIT

    # For the Makefile ...
    export PLATFORM
    export BACKEND_DIR

    if [ ! $REBUILD ]; then
        make clean
    fi

    make || fail "Failed to build AVbin."
}

build_macosx_universal() {
    if [ ! -e dist/macosx-x86-32/libavbin.$AVBIN_VERSION.dylib ]; then
        PLATFORM=macosx-x86-32
        build_backend
        build_avbin
    fi

    if [ ! -e dist/macosx-x86-64/libavbin.$AVBIN_VERSION.dylib ]; then
        PLATFORM=macosx-x86-64
        build_backend
        build_avbin
    fi

    mkdir -p dist/macosx-universal
    lipo -create \
        -output dist/macosx-universal/libavbin.$AVBIN_VERSION.dylib \
        dist/macosx-x86-32/libavbin.$AVBIN_VERSION.dylib \
        dist/macosx-x86-64/libavbin.$AVBIN_VERSION.dylib || fail "Failed to create universal shared library."
}

die_usage() {
    echo "Usage: ./build.sh [options] <platform> [<platform> [<platform> ...]]"
    echo
    echo "Options"
    echo "  --clean     Don't build, just clean up all generated files and directories."
    echo "  --fast      Use 'make -j4' when compiling"
    echo "  --help      Display this help text."
    echo "  --rebuild   Don't reconfigure, just run make again."
    echo
    echo "Supported platforms:"
    echo "  linux-x86-32"
    echo "  linux-x86-64"
    echo "  macosx-x86-32"
    echo "  macosx-x86-64"
    echo "  macosx-universal (builds all supported Mac OS X architectures into one library)"
    echo "  win32"
    echo "  win64"
    exit 1
}

for arg in $* ; do
    case $arg in
        "--fast")
            FAST="-j4" ;;
        "--help")
            die_usage ;;
        "--rebuild")
            REBUILD=1;;
        "--clean")
            clean_backend
            rm -rf dist
            rm -rf build
            rm -f example/avbin_dump
            exit
            ;;
        *)
            if [ "$platforms" == "" ]; then
                platforms=$arg
            else
                platforms="$platforms $arg"
            fi
            ;;
    esac
done;

if [ "$platforms" == "" ]; then
    die_usage
fi

for PLATFORM in $platforms; do
    case $PLATFORM in
        "macosx-universal")
            OSX_VERSION=`/usr/bin/sw_vers -productVersion | cut -b 1-4`
            build_macosx_universal
            ;;
        "macosx-x86-32" | "macosx-x86-64")
            OSX_VERSION=`/usr/bin/sw_vers -productVersion | cut -b 1-4`
            build_backend
            build_avbin
            ;;
        "linux-x86-32" | "linux-x86-64" | "win32" | "win64")
            build_backend
            build_avbin
            ;;
        *)
            echo "Unrecognized platform: $PLATFORM.  Try ./build.sh --help" && exit 3
            ;;
    esac
done

echo "Build successful."
