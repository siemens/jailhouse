#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2016
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

ifneq ($(SRCARCH),x86)
DTS = $(shell cd $(src); ls *.dts)
endif

always := $(DTS:.dts=.dtb)

targets += $(DTS:.dts=.dtb)
