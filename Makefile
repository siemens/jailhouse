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

# no recipes above this one (also no includes)
all: modules tools

# includes installation-related variables and definitions
include scripts/install.mk

# out-of-tree build for our kernel-module, firmware and inmates
KDIR ?= /lib/modules/`uname -r`/build

define run-kbuild =
	$(MAKE) -C $(KDIR) M=$$PWD $@
endef

modules:
	$(run-kbuild)

hypervisor/jailhouse.bin: modules

# recursive build of tools
tools:
	$(MAKE) -C tools

# clean up kernel and tools
clean:
	$(run-kbuild)
	$(MAKE) -C tools $@

modules_install: modules
	$(run-kbuild)
	depmod -aq

firmware_install: hypervisor/jailhouse.bin $(DESTDIR)$(firmwaredir)
	$(INSTALL_DATA) $^

install: modules_install firmware_install
	$(MAKE) -C tools $@

.PHONY: modules_install install clean firmware_install modules tools
