#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013-2015
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Benjamin Block <bebl@mageta.org>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

subdir-y := driver hypervisor configs inmates

subdir-ccflags-y := -Werror

# inmates build depends on generated config.mk of the hypervisor,
# and the driver needs version.h from there
$(obj)/inmates $(obj)/driver: $(obj)/hypervisor
