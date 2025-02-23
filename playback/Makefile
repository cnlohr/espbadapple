include ../common/make.inc

all : playback

CFLAGS:=-I../common -I../vpxtest -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE} -DFRAMECT=${FRAMECT}

CFLAGS+=-Os -g -Wno-unused-result
LDFLAGS:=-lX11 -lm -lGL
LDFLAGS+=-Wl,--gc-sections -flto -ffunction-sections -fdata-sections -fmessage-length=0 -Wl,--print-memory-usage -Wl,-Map=playback.map

playback : playback.c ba_play.h ../vpxtest/badapple_data.h
	gcc -o $@ $< $(CFLAGS) $(LDFLAGS)
	objdump -S $@ > $@.lst


playback.gif : playback
	./playback

playback.mp4 : playback.gif
	ffmpeg -loglevel quiet -y -f f32le -ar 48000 -ac 1 -i audio.dump -r 30 -i playback.gif -af adelay=1350 -vf "scale=trunc(iw/2)*4:trunc(ih/2)*4" -r 30 -crf 15 playback.mp4

clean :
	rm -rf playback

