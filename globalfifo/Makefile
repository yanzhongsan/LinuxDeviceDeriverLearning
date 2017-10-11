PWD = $(shell pwd)
KERNEL_SRC = /usr/src/linux-headers-4.10.0-36-generic/

obj-m := globalfifo.o
module-objs := globalfifo.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules


clean:
	rm *.ko
	rm *.o
