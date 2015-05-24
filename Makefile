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

# Check make version
need := 3.82
ifneq ($(need),$(firstword $(sort $(MAKE_VERSION) $(need))))
$(error Too old make version $(MAKE_VERSION), at least $(need) required)
endif

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

# recursive build of tools
tools:
	$(Q)$(MAKE) -C tools

# documentation, build needs to be triggered explicitly
docs:
	$(DOXYGEN) Documentation/Doxyfile

# remove generated docs
docs_clean:
	rm -rf Documentation/generated

# clean up kernel, tools and generated docs
clean:	docs_clean
	$(Q)$(MAKE) $(kbuild)
	$(Q)$(MAKE) -C tools $@

modules_install: modules
	$(Q)$(MAKE) $(kbuild)

firmware_install: $(DESTDIR)$(firmwaredir) modules
	$(INSTALL_DATA) hypervisor/jailhouse*.bin $<

ifeq ($(ARCH),x86)
TOOL_INMATES_INSTALL := tool_inmates_install
tool_inmates_install: $(DESTDIR)$(libexecdir)/jailhouse
	$(INSTALL_DATA) inmates/tools/$(ARCH)/*.bin $<
endif

install: modules_install firmware_install $(TOOL_INMATES_INSTALL)
	$(Q)$(MAKE) -C tools $@

.PHONY: modules_install install clean firmware_install modules tools docs \
	docs_clean
