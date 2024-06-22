all : testsynth midiexport fmcomp bitlzsstest

bitlzsstest : bitlzsstest.c
	gcc -O2 -g -o $@ $^ -lm

midiexport : midiexport.c
	gcc -O2 -g -o $@ $^ -lm

fmraw.dat : midiexport BadApple-mod.mid
	./midiexport < BadApple-mod.mid

test : testsynth fmraw.dat fmcomp
	./fmcomp
	./testsynth  > track-float-48000.dat
	./testsynth  | aplay -D pulse -c 1 -t raw -r 48000 -f FLOAT_LE

fmcomp : fmcomp.c
	gcc -O2 -g -o $@ $^ -lm

testsynth : testsynth.c
	gcc -O2 -g -o $@ $^ -lm

clean :
	rm -rf *.o ~* midiexport testsynth bitlzsstest
