#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013, 2014
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Banjmin Block <bebl@mageta.org>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

subdir-y := hypervisor configs inmates

obj-m := jailhouse.o

ccflags-y := -I$(src)/hypervisor/arch/$(SRCARCH)/include \
	     -I$(src)/hypervisor/include

jailhouse-y := driver.o

define filechk_version
	$(src)/scripts/gen_version_h $(src)
endef

clean-files := jailhouse-version.h

$(obj)/jailhouse-version.h: $(src)/Makefile FORCE
	$(call filechk,version)

$(obj)/driver.o: $(obj)/jailhouse-version.h
