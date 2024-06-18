all : decodevideo

#  -fsanitize=address -static-libasan
LDFLAGS:=-g -lX11 -lm
RESX=64
RESY=48
BLOCKSIZE=8
RES:=${RESX}x${RESY}x${BLOCKSIZE}

CFLAGS:=-O2 -g -Wno-unused-result -mavx2 -DBLOCKSIZE=${BLOCKSIZE}

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

streamcomp : streamcomp.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

video-${RES}.gif : streamcomp stream-${RES}.dat tiles-${RES}.dat
	./streamcomp stream-${RES}.dat tiles-${RES}.dat video-${RES}.gif ${RESX} ${RESY}

midclean :
	rm -rf test-${RES}.gif streamcomp

clean : midclean
	rm -rf decodevideo videocomp videoout-${RES}.dat
	rm -rf *~ 


## For video
#ffmpeg -i video-64x48x8.gif -vf "scale=trunc(iw/2)*4:trunc(ih/2)*4" -r 60 test.mp4
# or to slice in: ffmpeg -i track-float-48000.mp3 -i video-64x48x8.gif -vf "scale=trunc(iw/2)*4:trunc(ih/2)*4" -r 60 test.mp4
# ffmpeg -y -f f32le -ar 48000 -ac 1 -i ../song/track-float-48000.dat -r 30 -i video-64x48x8.gif -af adelay=1000 -vf "scale=trunc(iw/2)*4:trunc(ih/2)*4" -r 30 test.mp4

