# $Id:$

# Makefile for avbin_dump.c.  Requires Linux or OS X (modifications for other
# platforms should be straightforward).

CC=gcc
CFLAGS=-I ../include -I ../libav -g
LIBS=-lavbin -lm -lpthread

all : avbin_dump

avbin_dump : avbin_dump.c
	$(CC) $(CFLAGS) avbin_dump.c -o avbin_dump $(LIBS)
