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

always := lib.a

ccflags-y := -ffunction-sections

lib-y				:= header.o gic.o printk.o timer.o \
				   uart-pl011.o uart-8250.o
lib-y				+= ../string.o ../cmdline.o
lib-$(CONFIG_ARM_GIC_V2)	+= gic-v2.o
lib-$(CONFIG_ARM_GIC_V3)	+= gic-v3.o
