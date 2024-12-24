all : vpxtest

include ../comp2/make.inc

CFLAGS:=-O2 -g -DRESX=$(RESX) -DRESY=$(RESY) -DBLOCKSIZE=$(BLOCKSIZE)

LDFLAGS:=-lm -lX11

vpxtest : vpxtest.c
	gcc -o $@ $^ $(LDFLAGS) $(CFLAGS) -lm

clean :
	rm -rf vpxtest

