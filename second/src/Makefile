PWD = $(shell pwd)
KVER = $(shell uname -r)
KERNEL_SRC = /usr/src/linux-headers-$(KVER)/

obj-m := second.o
module-objs := second.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules


clean:
	rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions
