all: *.c *.h
	gcc -O0 -g  -Wall -pthread -D_GNU_SOURCE  *.c -lm -levent -o loader

clean:
	rm loader
