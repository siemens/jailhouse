#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013, 2014
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Benjamin Block <bebl@mageta.org>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

subdir-y := hypervisor configs inmates

obj-m := jailhouse.o

ccflags-y := -I$(src)/hypervisor/arch/$(SRCARCH)/include \
	     -I$(src)/hypervisor/include

jailhouse-y := driver.o

$(obj)/hypervisor/include/jailhouse/version.h: $(obj)/hypervisor
	@

$(obj)/driver.o: $(obj)/hypervisor/include/jailhouse/version.h

# inmates build depends on generated config.mk of the hypervisor
ifeq ($(clean),)
$(addprefix $(obj)/,inmates): $(obj)/hypervisor
endif
