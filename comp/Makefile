all : rletest tiletest tiletest16

CFLAGS:=-O2 -g
LDFLAGS:=-g
LDLIBS:=-lavcodec -lavformat -lswscale -lavutil -lX11 -lXinerama -lXext

rletest : rletest.o ffmdecode.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

tiletest :  tiletest.o ffmdecode.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

tiletest16 :  tiletest16.o ffmdecode.o XDriver.o DrawFunctions.o gifenc.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean :
	rm -rf *.o *~ dectest
