all : decodevideo

CFLAGS:=-O2 -g -Wno-unused-result -mavx2
LDFLAGS:=-g -lX11 -lm

hufftreegentest : hufftreegentest.c
	gcc -g -O0 -o $@ $^

decodevideo : ffmdecode.o decodevideo.o
	gcc -g -O0 -o $@ $^ -lavcodec -lavformat -lswscale -lavutil  $(CFLAGS) $(LDFLAGS)

videoout-64x48.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo $< 64 48 $@

videocomp : videocomp.c gifenc.c
	gcc -g -O4 -o $@ $^ $(CFLAGS) $(LDFLAGS)

test-64x48.gif : videoout-64x48.dat videocomp 
	./videocomp $< 64 48 $@


midclean :
	rm -rf test-64x48.gif videocomp

clean : midclean
	rm -rf decodevideo videoout-64x48.dat
	rm -rf *~ 

