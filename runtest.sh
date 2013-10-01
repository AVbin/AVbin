#!/bin/bash

# runtest.sh
# Copyright 2013 AVbin Team
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


# This should be supplied as an argument to 'git bisect'.
#
# Of course, this only works if you set these two variables correctly, and
# you're debugging a problem on 64-bit OS X.  The script will need to be
# modified under any other circumstances.
PROBLEM_MEDIA_FILE=~/proj/AVbin10_problems/jwpIntro.mov 
AVBIN_APPLICATION=~/proj/pyglet/examples/media_player.py 


die_usage() {
    echo "Usage: ./runtest.sh [options] [media_file]"
    echo ""
    echo "Run a test video after (optionally) rebuilding libav and avbin."
    echo ""
    echo "Options:"
    echo "    --avbin  Rebuild avbin."
    echo "    --libav  Rebuild libav and avbin."
    echo "    --help   Display this help message."
    exit 3
}
 
LIBAV="no"
AVBIN="no"

for arg in $* ; do
    case $arg in
        "--avbin")
            AVBIN="yes" ;;
        "--libav")
            LIBAV="yes" ;;
        "--help")
            die_usage ;;
        *)
            if [[ -r "$arg" ]] ; then
                PROBLEM_MEDIA_FILE="$arg"
            else
                echo "'$arg' is not a readable file"
                die_usage
            fi
            ;;
    esac
done;

if [ $LIBAV == "yes" ] ; then
    ./build.sh --clean || exit 4
    ./build.sh --fast macosx-x86-64 || exit 5
    ./dist.sh macosx-x86-64 || exit 6
    sudo cp dist/macosx-x86-64/libavbin.11.dylib /usr/local/lib/libavbin.11.dylib || exit 6
elif [ $AVBIN == "yes" ] ; then
    ./build.sh --rebuild --fast macosx-x86-64 || exit 7
    ./dist.sh macosx-x86-64 || exit 8
    sudo cp dist/macosx-x86-64/libavbin.11.dylib /usr/local/lib/libavbin.11.dylib || exit 9
fi
    
$AVBIN_APPLICATION $PROBLEM_MEDIA_FILE || exit 10
