# For other settings, see bacommon.h

RESX=64
RESY=48
BLOCKSIZE=8
TARGET_GLYPH_COUNT=256
RES:=${RESX}x${RESY}x${BLOCKSIZE}
FRAMECT:=6570

ifeq ($(OS),Windows_NT)
	LDFLAGS:=-lkernel32 -lgdi32 -luser32 -lm -g
else
	LDFLAGS:=-lm -lX11
endif


CFLAGS:=-O4 -g -I../common -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE} -DFRAMECT=${FRAMECT} -DTARGET_GLYPH_COUNT=${TARGET_GLYPH_COUNT}

ifeq ($(OS),Windows_NT)
    FFMPEG_CFLAGS := -I../common/ffmpegwindows
else
    FFMPEG_CFLAGS := 
endif
