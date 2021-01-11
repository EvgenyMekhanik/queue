all: test

test: main.o module_api.o
	gcc main.o module_api.o -o test -luv -lpthread

main.o: main.c
	gcc -c main.c

module_api.o: module_api.c
	gcc -c module_api.c

clean:
	rm -rf *.o test