CC      = g++
WFLAGS  = -W -Wall 
INCLUDE = -isystem /lib/modules/`uname -r`/build/include
CFLAGS  = ${WFLAGS} ${INCLUDE} -g

all : self timing

clean :
	rm -f *.o self

self: self.o
	$(CC) $(CFLAGS) self.o -o self

self.o: self.c
	$(CC) $(CFLAGS) -c self.c -o self.o

timing: timing.o
	$(CC) $(CFLAGS) timing.o -o timing

timing.o: timing.c
	$(CC) $(CFLAGS) -c timing.c -o timing.o
