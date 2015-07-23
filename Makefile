HOST_EXTRACFLAGS += -isystem /lib/modules/$(shell uname -r)/build/include

obj-m += cr3spy_mod.o

all: module util

util:
	make -C probe

module: cr3spy_mod.c cr3spy_mod.h
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
	make -C probe clean
