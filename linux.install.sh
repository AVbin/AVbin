#!/usr/bin/env bash
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

PREFIX=/usr/lib
AVBIN_LIBRARY=libavbin.so.@AVBIN_VERSION@

fail() {
    echo "$1"
    exit 1
}

touch $PREFIX 2> /dev/null || \
    fail "Insufficient priveleges to install AVbin.  Run installer with 'sudo' or as root."

echo
echo "The full source code for AVbin can be obtained from avbin.github.org"
echo

# Show the license and get user acceptance
cat COPYING.LESSER
echo
read -p "Do you agree to the terms of the license above? (Y/n) "
if [ "$REPLY" == "y" -o "$REPLY" == "Y" -o "$REPLY" == "yes" -o "$REPLY" == "" ] ; then
    echo
else
    fail "Installation aborted: License not accepted"
fi

cp $AVBIN_LIBRARY $PREFIX/ || fail "Unable to copy $AVBIN_LIBRARY to $PREFIX/"
ln -sf $AVBIN_LIBRARY $PREFIX/libavbin.so || \
    fail "Unable to create symlink $PREFIX/libavbin.so -> $AVBIN_LIBRARY"
chmod a+rx $PREFIX/$AVBIN_LIBRARY || \
    fail "Unable to chmod a+rx $PREFIX/$AVBIN_LIBRARY"
/sbin/ldconfig || \
    echo "WARNING: Unable to update dynamic linker run-time bindings.  You may need to reboot before AVbin can be used."

echo "AVbin @AVBIN_VERSION_STRING@ successfully installed."
echo
