ifneq ($(KERNELRELEASE),)
include Kbuild
else

SRC		:= $(shell pwd)
ARCH		?= arm64
CROSS_COMPILE	?= aarch64-none-linux-gnu-
KERNEL_SRC	?= /lib/modules/$(shell uname -r)/build

ifeq ($(DEBUG), 1)
ATP_CFLAGS	:= -g3
endif

.PHONY: default modules modules_install clean

default: modules

modules modules_install:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) ATP_CFLAGS=$(ATP_CFLAGS) -C $(KERNEL_SRC) M=$(SRC) $@

clean:
	$(RM) *.a *.o *.ko *.mod *.mod.* .*.cmd modules.order Module.symvers
endif
