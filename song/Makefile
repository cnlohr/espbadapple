all : testsynth midiexport fmcomp-vpx track-float-48000.dat

CFLAGS:=-O2 -g -I../common

midiexport : midiexport.c
	gcc $(CFLAGS) -o $@ $^ -lm

fmraw.dat : midiexport BadApple-mod.mid
	./midiexport < BadApple-mod.mid

track-float-48000.dat : testsynth fmraw.dat
	./testsynth  > track-float-48000.dat

test : testsynth fmraw.dat fmcomp-vpx
	./fmcomp-vpx
	./testsynth  | aplay -D pulse -c 1 -t raw -r 48000 -f FLOAT_LE

espbadapple_song.h : fmcomp-vpx fmraw.dat
	./fmcomp-vpx

fmcomp-vpx : fmcomp-vpx.c
	gcc $(CFLAGS) -o $@ $^ -lm

fmcomp : fmcomp.c
	gcc $(CFLAGS) -o $@ $^ -lm

testsynth : testsynth.c
	gcc $(CFLAGS) -o $@ $^ -lm

clean :
	rm -rf *.o ~* midiexport fmcomp fmcomp-vpx testsynth bitlzsstest huffTN_fmraw.dat huffTL_fmraw.dat huffD_fmraw.dat fmraw.dat track-float-48000.dat
