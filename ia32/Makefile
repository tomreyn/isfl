#
# Makefile for InsydeFlash tool for Linux (build your main kernel)
#
MACHINE = $(shell uname -m)

obj-m+=isfl_drv.o


all: isfl_drv

isfl_drv:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules

clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean

