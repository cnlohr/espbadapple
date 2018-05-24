all : decodevideo rletest tiletest16 test_tilehuff

CFLAGS:=-O2 -g
LDFLAGS:=-g
LDLIBS:=-lX11 -lXinerama -lXext

decodevideo : ffmdecode.o decodevideo.o
	gcc -g -O0 -o $@ $^ -lavcodec -lavformat -lswscale -lavutil 

rletest : rletest.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

tiletest16 :  tiletest16.o XDriver.o DrawFunctions.o gifenc.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

test_tilehuff : test_tilehuff.c outdata.c XDriver.c DrawFunctions.c gifenc.c
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean :
	rm -rf *.o *~ dectest
