include ../common/make.inc

all : playback

CFLAGS:=-I../common -I../vpxtest -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE} -DFRAMECT=${FRAMECT}

CFLAGS+=-Og -g
LDFLAGS:=-lX11 -lm -lGL

playback : playback.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf playback

