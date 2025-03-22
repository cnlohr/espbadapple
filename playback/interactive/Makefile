all : interactive

include ../../common/make.inc

CFLAGS+=-I. -I.. -I../../common -I../../vpxtest
LDFLAGS+=-lm -lGL

interactive : interactive.c
	gcc -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean :
	rm -rf interactive

