all : testsynth midiexport fmcomp-vpx-lzss track-float-48000.dat

CFLAGS:=-Og -g -I../common

midiexport : midiexport.c
	gcc $(CFLAGS) -o $@ $^ -lm

fmraw.dat : midiexport BadApple-mod.mid
	./midiexport < BadApple-mod.mid

track-float-48000.dat : testsynth fmraw.dat
	./testsynth  > track-float-48000.dat

test : testsynth fmraw.dat fmcomp-vpx-lzss
	./fmcomp-vpx-lzss
	./testsynth  | aplay -D pulse -c 1 -t raw -r 48000 -f FLOAT_LE

../playback/badapple_song.h : fmcomp-vpx-lzss fmraw.dat
	./fmcomp-vpx-lzss

fmcomp-vpx-lzss : fmcomp-vpx-lzss.c
	gcc $(CFLAGS) -o $@ $^ -lm

test-vpx-lzss : test-vpx-lzss.c
	gcc $(CFLAGS) -o $@ $^ -lm -I../playback

testsynth : testsynth.c
	gcc $(CFLAGS) -o $@ $^ -lm


# Another try with huffman

fmcomp-huffman-reverselzss2 : fmcomp-huffman-reverselzss2.c
	gcc $(CFLAGS) -o $@ $^ -lm

../playback/badapple_song_huffman_reverselzss.h : fmcomp-huffman-reverselzss2 fmraw.dat
	./fmcomp-huffman-reverselzss2

COMPLIST:=fmraw.dat.json.jq fmraw.dat.json fmraw.dat.xml fmraw.dat BadApple-mod.mid

sizecomp : fmraw.dat heatshrink
	cat fmraw.dat.json | jq > fmraw.dat.json.jq
	for s in $(COMPLIST); do \
		echo $$s ; \
		cat $$s | wc ; \
		cat $$s | gzip -9 | wc ; \
		cat $$s | zstd -9 | wc ; \
		cat $$s | bzip2 -9 | wc ; \
		cat $$s | heatshrink/heatshrink | wc; \
	done
	echo ""
	gcc attic/fmcomp-singletable.c -o attic/fmcomp-singletable -I../common -lm
	gcc attic/fmcomp-huffman.c -o attic/fmcomp-huffman -I../common -lm
	gcc attic/fmcomp-vpx.c -o attic/fmcomp-vpx -I../common -lm
	gcc attic/fmcomp-tripletable.c -o attic/fmcomp-tripletable -I../common -lm
	attic/fmcomp-singletable
	attic/fmcomp-huffman
	attic/fmcomp-tripletable
	attic/fmcomp-vpx

heatshrink :
	git clone https://github.com/atomicobject/heatshrink
	cd heatshrink; make

clean :
	rm -rf *.o ~* test-vpx-lzss midiexport fmcomp fmcomp-vpx fmcomp-vpx-lzss testsynth bitlzsstest huffTN_fmraw.dat huffTL_fmraw.dat huffD_fmraw.dat fmraw.dat track-float-48000.dat ../playback/badapple_song.h
