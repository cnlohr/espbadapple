all : dectest

CFLAGS:=-O2 -g
LDFLAGS:=-g
LDLIBS:=-lavcodec -lavformat -lswscale -lavutil -lX11 -lXinerama -lXext

dectest : dectest.o ffmdecode.o XDriver.o DrawFunctions.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean :
	rm -rf *.o *~ dectest
