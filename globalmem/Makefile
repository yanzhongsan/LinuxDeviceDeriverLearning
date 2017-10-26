PWD = $(shell pwd)
KVER = $(shell uname -r)
KERNEL_SRC = /usr/src/linux-headers-$(KVER)/

obj-m := globalmem.o
module-objs := globalmem.o

all:
	$(MAKE) -C $(KERNEL_SRC) M=$(PWD) modules


clean:
	rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions
