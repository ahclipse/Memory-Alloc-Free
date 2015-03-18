all: mem

mem: mem.c mymem.h test.c
	gcc -c -fpic mem.c -Wall -g -pthread
	gcc -shared -o libmem.so mem.o
	export LD_LIBRARY_PATH=.
	gcc -lmem -L. -o tester test.c -Wall -g

clean:
	rm -rf mem.o libmem.so test.o tester
