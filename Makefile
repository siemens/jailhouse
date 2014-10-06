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
include scripts/include.mk

# out-of-tree build for our kernel-module, firmware and inmates
KDIR ?= /lib/modules/`uname -r`/build

INSTALL_MOD_PATH ?= $(DESTDIR)
export INSTALL_MOD_PATH

DOXYGEN ?= doxygen

kbuild = -C $(KDIR) M=$$PWD $@

modules:
	$(Q)$(MAKE) $(kbuild)

hypervisor/jailhouse.bin: modules

# recursive build of tools
tools:
	$(Q)$(MAKE) -C tools

# documentation, build needs to be triggered explicitly
docs:
	$(DOXYGEN) Documentation/Doxyfile

# clean up kernel, tools and generated docs
clean:
	$(Q)$(MAKE) $(kbuild)
	$(Q)$(MAKE) -C tools $@
	rm -rf Documentation/generated

modules_install: modules
	$(Q)$(MAKE) $(kbuild)

firmware_install: hypervisor/jailhouse.bin $(DESTDIR)$(firmwaredir)
	$(INSTALL_DATA) $^

install: modules_install firmware_install
	$(Q)$(MAKE) -C tools $@

.PHONY: modules_install install clean firmware_install modules tools docs
