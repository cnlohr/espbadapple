include ../common/make.inc

all : streamrecomp

streamrecomp : streamrecomp.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

stream_stripped.dat : streamrecomp
	./streamrecomp

clean :
	rm -rf *.o *~ streamrecomp stream_stripped.dat

