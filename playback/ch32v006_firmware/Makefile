include ../../common/make.inc

PREFIX:=~/Downloads/xpack-riscv-none-elf-gcc-14.2.0-3/bin/riscv-none-elf
OBJCOPY_FLAGS:=-j .init -j .text
WRITE_SECTION:=0x1fff0000

CFLAGS= -DRESX=${RESX} -DRESY=${RESY} -DBLOCKSIZE=${BLOCKSIZE} -DFRAMECT=${FRAMECT} -DTARGET_GLYPH_COUNT=${TARGET_GLYPH_COUNT}
LDFLAGS=
CFLAGS+=-I.. -I../../common -I../../vpxtest -mrelax -ffunction-sections -flto -Os -ffunction-sections -fdata-sections -fmessage-length=0 -msmall-data-limit=8
LINKER_SCRIPT:=ch32v006_split.ld

all : flash

TARGET:=badappleplay
TARGET_MCU?=CH32V006
include ../ch32v003fun/ch32fun/ch32fun.mk

optionbytes:
	#$(MINICHLINK)/minichlink -w +a55aff00 option # Enable bootloader, disable RESET
	#$(MINICHLINK)/minichlink -w +a55aff00 option # Enable bootloader, enable RESET
	#$(MINICHLINK)/minichlink -w +a55aef10 option # Enable bootloader, enable RESET (Alternate)
	$(MINICHLINK)/minichlink -w +a55adf20 option # Disable bootloader, disable RESET
	#$(MINICHLINK)/minichlink -w +a55acf30 option # Disable bootloader, enable RESET

enablebootloader:
	$(MINICHLINK)/minichlink -w +a55aff00 option # Enable bootloader, disable RESET
disablebootloader:
	$(MINICHLINK)/minichlink -w +a55adf20 option # Disable bootloader, disable RESET

flashdata :
	$(PREFIX)-objcopy -j .fixedflash -O binary $(TARGET).elf fixedflash.bin
	$(MINICHLINK)/minichlink -w fixedflash.bin 0x08000000

FLASH_COMMAND:=$(CH32FUN)/../minichlink/minichlink -a -U -w $(TARGET).bin $(WRITE_SECTION) -B

flash : cv_flash
clean : cv_clean

