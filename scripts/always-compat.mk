#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2020
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Benjamin Block <bebl@mageta.org>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

ifeq ($(shell expr \( $(VERSION) \* $$((0x100)) \+ $(PATCHLEVEL) \) \< $$((0x509))),1)
always = $(always-y)
endif
