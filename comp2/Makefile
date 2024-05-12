all : decodevideo

CFLAGS:=-O2 -g -Wno-unused-result -mavx2
LDFLAGS:=-g -lX11 -lm
RESX=96
RESY=64
RES:=${RESX}x${RESY}

hufftreegentest : hufftreegentest.c
	gcc -g -O0 -o $@ $^

decodevideo : ffmdecode.o decodevideo.o
	gcc -g -O0 -o $@ $^ -lavcodec -lavformat -lswscale -lavutil  $(CFLAGS) $(LDFLAGS)

videoout-${RES}.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo $< ${RESX} ${RESY} $@

videocomp : videocomp.c gifenc.c
	gcc -g -O4 -o $@ $^ $(CFLAGS) $(LDFLAGS)

test-${RES}.gif : videoout-${RES}.dat videocomp 
	./videocomp $< ${RESX} ${RESY} $@


midclean :
	rm -rf test-${RES}.gif videocomp

clean : midclean
	rm -rf decodevideo videoout-${RES}.dat
	rm -rf *~ 

