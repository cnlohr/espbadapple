all : decodevideo rletest rle255 tiletest16 test_tilehuff example.gif

CFLAGS:=-O2 -g -Wno-unused-result
LDFLAGS:=-g
LDLIBS:=-lX11

decodevideo : ffmdecode.o decodevideo.o
	gcc -g -O0 -o $@ $^ -lavcodec -lavformat -lswscale -lavutil 

rletest : rletest.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS) $(CFLAGS)

rle255 : rle255.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS) $(CFLAGS)

tiletest16 :  tiletest16.o gifenc.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS) $(CFLAGS)

tiletest8 :  tiletest8.o gifenc.o
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS) $(CFLAGS)

test_tilehuff : test_tilehuff.c outdata.c gifenc.c
	gcc -o $@ $^ $(LDFLAGS) $(LDLIBS) $(CFLAGS)


videoout.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo badapple-sm8628149.mp4 504 384

outdata.c : videoout.dat tiletest16
	./tiletest16 1 badapple-sm8628149.mp4
	./tiletest16 2 2 -1
	./tiletest16 3 20000 .001
	./tiletest16 2 2 .005
	./tiletest16 3 20000 .001
	./tiletest16 4

# To REALLY crank down
# After running the above
# ./tiletest16 2 2 .1 && ./tiletest16 3 20000 .15 && ./tiletest16 2 2 .1 && ./tiletest16 3 20000 .15 && ./tiletest16 4 && make example.gif

example.gif : test_tilehuff
	./test_tilehuff

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
	rm -rf *.o *~ dectest outdata.c example.gif videoout.dat test_tilehuff tiletest8 tiletest16

