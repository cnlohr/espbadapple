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
	./decodevideo badapple-sm8628149.mp4 320 240

outdata.c : videoout.dat tiletest16
	# Stage 1 has no properties.  It only takes in a video (which is actually videoout.dat) and outputs rawtiledata.dat
	./tiletest16 1 #badapple-sm8628149.mp4
	# Stage 2 [cutoff] [dynamic cutoff]
	#  Where the cutoff value is defined by cutoff_mark = cutoff + cutoff_dyn * i+1;
	./tiletest16 2 2 -1
	# Stage 3 [tileout] [weight_toward_earlier_symbols]
	#  Tileout is the max # of tiles we are allowed to use, i.e. cut off all but this #.
	#  weight_toward_earlier_symbols --> How much should we prefer earlier symbols?
	#  Note, see USE_PREVIOUS_THRESH for situations where we try to just re-use the tile that was already there.
	./tiletest16 3 20000 .001
	./tiletest16 2 2 .001  # Was 0.002
	./tiletest16 3 20000 .001
	# Produce final output.
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

