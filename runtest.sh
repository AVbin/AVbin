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

./build.sh --clean || exit 3
./build.sh --fast macosx-x86-64 || exit 4
./dist.sh macosx-x86-64 || exit 5
sudo cp dist/macosx-x86-64/libavbin.11.dylib /usr/local/lib/libavbin.11.dylib || exit 6
$AVBIN_APPLICATION $PROBLEM_MEDIA_FILE || exit 7
