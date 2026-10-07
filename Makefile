# SPDX-License-Identifier: GPL-2.0
# Build against a kernel tree without DKMS, e.g. while developing:
#   make KDIR=/path/to/linux [ARCH=riscv CROSS_COMPILE=riscv64-linux-gnu-]
KDIR ?= /lib/modules/$(shell uname -r)/build

modules clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) $@

.PHONY: modules clean
