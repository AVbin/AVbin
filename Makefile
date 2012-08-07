# Makefile
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

CFLAGS += -DAVBIN_VERSION=$(AVBIN_VERSION) \
          -DAVBIN_VERSION_STRING='$(AVBIN_VERSION_STRING)' \
          -DAVBIN_BUILD_DATE='$(AVBIN_BUILD_DATE)' \
          -DAVBIN_REPO='$(AVBIN_REPO)' \
          -DAVBIN_COMMIT='$(AVBIN_COMMIT)' \
          -DAVBIN_BACKEND='$(AVBIN_BACKEND)' \
          -DAVBIN_BACKEND_VERSION_STRING='$(AVBIN_BACKEND_VERSION_STRING)' \
          -DAVBIN_BACKEND_REPO='$(AVBIN_BACKEND_REPO)' \
          -DAVBIN_BACKEND_COMMIT='$(AVBIN_BACKEND_COMMIT)'

CC = gcc
LD = ld
BUILDDIR = build
OUTDIR = dist/$(PLATFORM)

OBJNAME = $(BUILDDIR)/avbin.o

INCLUDE_DIRS = -I include \
               -I $(BACKEND_DIR)

include $(PLATFORM).Makefile

all : $(LIBNAME)
	ln -sf $(LIBNAME) $(LINKNAME)

$(OBJNAME) : src/avbin.c include/avbin.h $(BUILDDIR)
	$(CC) -c $(CFLAGS) $(INCLUDE_DIRS) -o $@ $<

$(BUILDDIR) :
	mkdir -p $(BUILDDIR)

$(OUTDIR) :
	mkdir -p $(OUTDIR)

clean : 
	rm -f $(OBJNAME)
	rm -f $(LINKNAME)
	rm -f $(LIBNAME)
