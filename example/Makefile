# $Id:$

# Makefile for avbin_dump.c.  Requires Linux or OS X (modifications for other
# platforms should be straightforward).

CC=gcc
CFLAGS=-I ../include -I ../libav
LIBS=-lavbin -lm -lpthread

all : minimal avbin_dump

minimal : minimal.c
	$(CC) $(CFLAGS) minimal.c -o minimal $(LIBS)

avbin_dump : avbin_dump.c
	$(CC) $(CFLAGS) avbin_dump.c -o avbin_dump $(LIBS)
