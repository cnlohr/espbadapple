include ../../common/make.inc

CFLAGS= -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE} -DFRAMECT=${FRAMECT} -DTARGET_GLYPH_COUNT=${TARGET_GLYPH_COUNT}
LDFLAGS=
CFLAGS+=-I.. -I../../common -I../../vpxtest -mrelax -ffunction-sections -flto -Os -ffunction-sections -fdata-sections -fmessage-length=0 -msmall-data-limit=8
LINKER_SCRIPT:=ch32v006_split.ld

all : flash

TARGET:=badappleplay
TARGET_MCU?=CH32V303
include ../ch32v003fun/ch32fun/ch32fun.mk

flash : cv_flash
clean : cv_clean

