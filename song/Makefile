all : testsynth midiexport fmcomp track-float-48000.dat

midiexport : midiexport.c
	gcc -O2 -g -o $@ $^ -lm

fmraw.dat : midiexport BadApple-mod.mid
	./midiexport < BadApple-mod.mid

track-float-48000.dat : testsynth fmraw.dat
	./testsynth  > track-float-48000.dat

test : testsynth fmraw.dat fmcomp
	./fmcomp
	./testsynth  | aplay -D pulse -c 1 -t raw -r 48000 -f FLOAT_LE

fmcomp : fmcomp.c
	gcc -O2 -g -o $@ $^ -lm

testsynth : testsynth.c
	gcc -O2 -g -o $@ $^ -lm

clean :
	rm -rf *.o ~* midiexport testsynth bitlzsstest huffTN_fmraw.dat huffTL_fmraw.dat huffD_fmraw.dat fmraw.dat track-float-48000.dat
