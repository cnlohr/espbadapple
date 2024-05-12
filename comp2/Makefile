all : decodevideo

CFLAGS:=-O2 -g -Wno-unused-result -mavx2
LDFLAGS:=-g -lX11 -lm
RESX=64
RESY=48
RES:=${RESX}x${RESY}

hufftreegentest : hufftreegentest.c
	gcc $(CFLAGS) -o $@ $^

decodevideo : ffmdecode.o decodevideo.o
	gcc $(CFLAGS) -o $@ $^ -lavcodec -lavformat -lswscale -lavutil  $(CFLAGS) $(LDFLAGS)

videoout-${RES}.dat : badapple-sm8628149.mp4 decodevideo
	./decodevideo $< ${RESX} ${RESY} $@

videocomp : videocomp.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

stream-${RES}.dat tiles-${RES}.dat : videoout-${RES}.dat videocomp 
	./videocomp $< ${RESX} ${RESY} tiles-${RES}.dat stream-${RES}.dat

streamcomp : streamcomp.c gifenc.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

video-${RES}.gif : streamcomp stream-${RES}.dat tiles-${RES}.dat
	./streamcomp stream-${RES}.dat tiles-${RES}.dat video-${RES}.gif

midclean :
	rm -rf test-${RES}.gif videocomp

clean : midclean
	rm -rf decodevideo videoout-${RES}.dat tiles-${RES}.dat stream-${RES}.dat
	rm -rf *~ 

