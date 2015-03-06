all: mem

mem: mem.c mymem.h test.c
	gcc -c -Wall -m32 -fpic mem.c -O
	gcc -c -Wall -m32 -fpic test.c test -O
	gcc -shared -Wall -m32 -o libmem.so mem.o -O

clean:
	rm -rf mem.o libmem.so
