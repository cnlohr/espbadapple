include ../common/make.inc

all : streamrecomp

CFLAGS:=-O4 -g -I../common -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE} -DFRAMECT=${FRAMECT} -DTARGET_GLYPH_COUNT=${TARGET_GLYPH_COUNT}
LDFLAGS:=-lX11

streamrecomp : streamrecomp.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ streamrecomp

