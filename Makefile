#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013, 2014
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

all: modules

# out-of-tree build
KDIR ?= /lib/modules/`uname -r`/build

modules modules_install clean:
	$(MAKE) -C $(KDIR) M=$$PWD $@

modules_install: modules

hypervisor/jailhouse.bin: modules

firmware_install: hypervisor/jailhouse.bin
	cp $< /lib/firmware/

install: modules_install firmware_install
	depmod -aq

.PHONY: modules_install install clean firmware_install modules
