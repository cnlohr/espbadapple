all : rletest tiletest tiletest16 test_tilehuff

CFLAGS:=-O2 -g
LDFLAGS:=-g
LDLIBS:=-lavcodec -lavformat -lswscale -lavutil -lX11 -lXinerama -lXext

rletest : rletest.o ffmdecode.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

tiletest :  tiletest.o ffmdecode.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

tiletest16 :  tiletest16.o ffmdecode.o XDriver.o DrawFunctions.o gifenc.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

test_tilehuff : test_tilehuff.c outdata.c XDriver.c DrawFunctions.c gifenc.c
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean :
	rm -rf *.o *~ dectest
