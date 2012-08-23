# $Id:$

# Makefile for avbin_dump.c.  Requires Linux or OS X (modifications for other
# platforms should be straightforward).

CC=gcc
CFLAGS=-I../include
LIBS=-lavbin -lm -lpthread

avbin_dump : avbin_dump.c
	$(CC) $(CFLAGS) avbin_dump.c -o avbin_dump $(LIBS)
