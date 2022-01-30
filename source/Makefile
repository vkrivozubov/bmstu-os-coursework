KBUILD_EXTRA_SYMBOLS = $(shell pwd)/Module.symverscd
ifneq ($(KERNELRELEASE),)
	obj-m := main_md.o mouse_md.o
else
	CURRENT = $(shell uname -r)
	KDIR = /lib/modules/$(CURRENT)/build
	PWD = $(shell pwd)

default:
	$(MAKE) -C $(KDIR) M=$(PWD) modules
	make cleanHalf

cleanHalf:
	rm -rf *.o *~ *.mod *.mod.c Module.* *.order  .tmp_versions
	
clean:
	make cleanHalf
	rm -rf *.ko .*.cmd .*.d

endif 