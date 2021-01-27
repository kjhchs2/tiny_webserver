tiny: tiny.o csapp.o
	gcc -o tiny tiny.o csapp.o -lpthread
csapp.o: csapp.c csapp.h
	gcc -c csapp.c tiny.o
tiny.o: csapp.h tiny.c
	gcc -c csapp.h tiny.c
