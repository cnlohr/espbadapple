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


videoout.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo badapple-sm8628149.mp4 128 96

outdata.c : videoout.dat tiletest16
	./tiletest16 1 badapple-sm8628149.mp4
	./tiletest16 2 2 -1
	./tiletest16 3 20000 .001
	./tiletest16 2 2 .005
	./tiletest16 3 20000 .001
	./tiletest16 4

#	./tiletest16 2 2 -1
#	./tiletest16 3  2000000 0
#	#./tiletest16 2 1 .0005 (maybe more?)
#	./tiletest16 3 200000 .001
#	./tiletest16 3 10000 .001
#	./tiletest16 2 3 -2
#
#				./tiletest16 3 badapple-sm8628149.mp4 200000 .002  # Get a sorted list out.
#				./tiletest16 3 badapple-sm8628149.mp4 200000 .002  # Get a sorted list out.
#			This gets you to under 2MB but you can keep going...  This starts to look awful.
#				./tiletest16 2 2 .004 (maybe more?)
#				./tiletest16 3 badapple-sm8628149.mp4 200000 .002  # Get a sorted list out.
#			Gets you to 1.7MB
#				./tiletest16 2 2 .02 (maybe more?)
#				./tiletest16 3 badapple-sm8628149.mp4 200000 .01  # Get a sorted list out.
#				./tiletest16 2 2 .03 (maybe more?)
#				./tiletest16 3 badapple-sm8628149.mp4 200000 .02  # Get a sorted list out.
#				./tiletest16 3 badapple-sm8628149.mp4 200000 .1  # Get a sorted list out.
#				./tiletest16 4




# look into these conrols.
# #define FOR_ESP8266
# #define SUPERTINY

clean :
	rm -rf *.o *~ dectest
