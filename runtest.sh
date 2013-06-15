#!/bin/bash

# This should be supplied as an argument to 'git bisect' that you run from
# *inside* the libav subdirectory.  Pass it as '../runtest.sh'
#
# Of course, this only works if you set these two variables correctly, and
# you're debugging a problem on 64-bit OS X.
PROBLEM_MEDIA_FILE=~/proj/AVbin10_problems/jwpIntro.mov 
AVBIN_APPLICATION=~/proj/pyglet/examples/media_player.py 

./build.sh --clean || exit 3
./build.sh --fast macosx-x86-64 || exit 4
./dist.sh macosx-x86-64 || exit 5
sudo cp dist/macosx-x86-64/libavbin.11.dylib /usr/local/lib/libavbin.11.dylib || exit 6
$AVBIN_APPLICATION $PROBLEM_MEDIA_FILE || exit 7
