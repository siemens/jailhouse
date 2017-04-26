#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015, 2016
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

include $(INMATES_LIB)/Makefile.lib
include $(INMATES_LIB)/../arm-common/Makefile.lib

always := lib.a inmate.lds

ccflags-y := -ffunction-sections

lib-y				:= $(COMMON_OBJECTS)
lib-y				+= header.o
lib-$(CONFIG_ARM_GIC_V3)	+= gic-v3.o
