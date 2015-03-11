all: mem

mem: mem.c mymem.h test.c
	gcc -c -Wall -m32 -fpic mem.c -O
	gcc -shared -Wall -m32 -o libmem.so mem.o -O
	gcc -lmem -L. -Wall -m32 test.c -O -o tester 

clean:
	rm -rf mem.o libmem.so tester
