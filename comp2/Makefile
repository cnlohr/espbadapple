all : decodevideo

CFLAGS:=-O2 -g -Wno-unused-result
LDFLAGS:=-g
LDLIBS:=-lX11

hufftreegentest : hufftreegentest.c
	gcc -g -O0 -o $@ $^

decodevideo : ffmdecode.o decodevideo.o
	gcc -g -O0 -o $@ $^ -lavcodec -lavformat -lswscale -lavutil 

videoout.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo badapple-sm8628149.mp4 96 64

videocomp : videocomp.c
	gcc -g -O0 -o $@ $^

example.gif : test_tilehuff
	./test_tilehuff


# #define FOR_ESP8266
# #define SUPERTINY

clean :
	rm -rf *.o *~ decodevideo videoout.dat

