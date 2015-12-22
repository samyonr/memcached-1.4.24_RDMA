CFLAGS = -g -O2 -pthread -pthread -Wall -pedantic -Wmissing-prototypes -Wmissing-declarations -Wredundant-decls

all: FailoverManager FailoverManagerTester

FailoverManager: FailoverManager.o sharedmalloc.o
	gcc $(CFLAGS) FailoverManager.o sharedmalloc.o -o FailoverManager
	
FailoverManager.o: FailoverManager.c
	gcc $(CFLAGS) -c FailoverManager.c -o FailoverManager.o

sharedmalloc.o: sharedmalloc.c
	gcc $(CFLAGS) -c sharedmalloc.c -o sharedmalloc.o

FailoverManagerTester : FailoverManagerTester.o sharedmalloc.o
	gcc $(CFLAGS) FailoverManagerTester.o sharedmalloc.o -o FailoverManagerTester
	
FailoverManagerTester.o : FailoverManagerTester.c
	gcc $(CFLAGS) -c FailoverManagerTester.c -o FailoverManagerTester.o

clean:
	rm -f FailoverManager.o
	rm -f FailoverManagerTester.o
	rm -f FailoverManager
	rm -f FailoverManagerTester
	rm -f *.dat
	rm -f *~