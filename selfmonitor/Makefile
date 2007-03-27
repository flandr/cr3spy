CC      = gcc
WFLAGS  = -W -Wall -Wstrict-prototypes
INCLUDE = -isystem /lib/modules/`uname -r`/build/include
CFLAGS  = ${WFLAGS} ${INCLUDE} -g

all : self

clean :
	rm -f *.o self

self: self.o
	$(CC) $(CFLAGS) self.o -o self

self.o: self.c
	$(CC) $(CFLAGS) -c self.c -o self.o
