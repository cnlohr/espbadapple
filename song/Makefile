all : testsynth midiexport

midiexport : midiexport.c
	gcc -O2 -g -o $@ $^ -lm

midtest : midiexport
	./midiexport < BadApple-mod.mid  > track-float-48000.dat
	./midiexport < BadApple-mod.mid  | aplay -D pulse -c 1 -t raw -r 48000 -f FLOAT_LE

testsynth : testsynth.c
	gcc -O2 -g -o $@ $^

clean :
	rm -rf *.o ~* midiexport testsynth
