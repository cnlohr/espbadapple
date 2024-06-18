all : testsynth

extractmidi : extractmidi.c
	gcc -o $@ $^

testsynth : testsynth.c
	gcc -o $@ $^

clean :
	rm -rf *.o ~*
