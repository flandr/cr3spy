CC      = gcc
WFLAGS  = -W -Wall -Wstrict-prototypes
CFLAGS  = ${WFLAGS} ${INCLUDE}

all : probe

clean :
	rm -f *.o probe

probe: probe.o
	$(CC) $(CFLAGS) probe.o -o probe

probe.o: probe.c
	$(CC) $(CFLAGS) -c probe.c -o probe.o
