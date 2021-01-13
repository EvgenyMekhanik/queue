all: test_send_recv test_dispatch_invoke

test_send_recv: main_send_recv.o module_api_send_recv.o
	gcc main_send_recv.o module_api_send_recv.o -o test_send_recv -luv -lpthread

test_dispatch_invoke: main_dispatch_invoke.o module_api_dispatch_invoke.o
	gcc main_dispatch_invoke.o module_api_dispatch_invoke.o -o test_dispatch_invoke -luv -lpthread

main_send_recv.o: main_send_recv.c
	gcc -c -g main_send_recv.c

main_dispatch_invoke.o: main_dispatch_invoke.c
	gcc -c -g main_dispatch_invoke.c

module_api_send_recv.o: module_api_send_recv.c
	gcc -c -g module_api_send_recv.c

module_api_dispatch_invoke.o: module_api_dispatch_invoke.c
	gcc -c -g module_api_dispatch_invoke.c

clean:
	rm -rf *.o test_send_recv test_dispatch_invoke