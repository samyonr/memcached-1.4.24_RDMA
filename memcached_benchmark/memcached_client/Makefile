all: *.c *.h
	gcc -O0 -g  -Wall -pthread -D_GNU_SOURCE  *.c -lm -L/usr/local/lib -levent -levent_pthreads -o loader

clean:
	rm loader
