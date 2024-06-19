all : testsynth midiexport

midiexport : midiexport.c
	gcc -O2 -g -o $@ $^ -lm

test : midiexport testsynth
	./midiexport < BadApple-mod.mid
	./testsynth  > track-float-48000.dat
	./testsynth  | aplay -D pulse -c 1 -t raw -r 48000 -f FLOAT_LE

testsynth : testsynth.c
	gcc -O2 -g -o $@ $^ -lm

clean :
	rm -rf *.o ~* midiexport testsynth
