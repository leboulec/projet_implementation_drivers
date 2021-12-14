obj-m:=dma_vga_driver.o
KDIR := ../build/buildroot-output/build/linux-xilinx-v2018.2/
all: default

default:
	$(MAKE) CROSS_COMPILE=../../host/bin/arm-linux- ARCH=arm -C $(KDIR) SUBDIRS=$(shell pwd) modules

clean:
	rm -f *.[oas] .*.flags *.ko .*.cmd .*.d .*.tmp *.mod.c