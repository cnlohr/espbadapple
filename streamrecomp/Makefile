include ../common/make.inc

all : streamrecomp

streamrecomp : streamrecomp.c
	gcc $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean :
	rm -rf *.o *~ streamrecomp

