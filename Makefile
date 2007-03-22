HOST_EXTRACFLAGS += -isystem /lib/modules/$(shell uname -r)/build/include

obj-m += cr3spy_mod.o

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
