all : decodevideo

CFLAGS:=-O2 -g -Wno-unused-result
LDFLAGS:=-g
LDLIBS:=-lX11

hufftreegentest : hufftreegentest.c
	gcc -g -O0 -o $@ $^

decodevideo : ffmdecode.o decodevideo.o
	gcc -g -O0 -o $@ $^ -lavcodec -lavformat -lswscale -lavutil 

videoout-64x48.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo $< 64 48 $@

videocomp : videocomp.c gifenc.c
	gcc -g -O4 -o $@ $^ -lX11

test-64x48.gif : videoout-64x48.dat videocomp 
	./videocomp $< 64 48 $@


# #define FOR_ESP8266
# #define SUPERTINY

clean :
	rm -rf *.o *~ decodevideo videoout-64x48.dat

