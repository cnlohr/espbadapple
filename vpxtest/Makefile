all : vpxtest

include ../common/make.inc

CFLAGS:=-O2 -g -DRESX=$(RESX) -DRESY=$(RESY) -DBLOCKSIZE=$(BLOCKSIZE) -Wno-unused-result -I../common

LDFLAGS:=-lm -lX11

vpxtest : vpxtest.c ../common/bacommon.h
	gcc -o $@ $< $(LDFLAGS) $(CFLAGS) -lm

test.gif : vpxtest
	./vpxtest ../comp2/stream-$(RESX)x$(RESY)x$(BLOCKSIZE).dat ../comp2/tiles-$(RESX)x$(RESY)x$(BLOCKSIZE).dat test.gif

test.mp4 : test.gif
	ffmpeg -loglevel quiet -y -f f32le -ar 48000 -ac 1 -i ../song/track-float-48000.dat -r 30 -i test.gif -af adelay=1350 -vf "scale=trunc(iw/2)*4:trunc(ih/2)*4" -r 30 -crf 15 test.mp4


clean :
	rm -rf vpxtest

