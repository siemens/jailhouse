#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

include $(INMATES_LIB)/Makefile.lib

always := lib.a

lib-y				:= header.o
lib-y				+= ../arm/gic.o ../arm/printk.o ../arm/timer.o
lib-y				+= ../string.o ../cmdline.o
lib-y				+= ../arm/uart-pl011.o ../arm/uart-8250.o
lib-$(CONFIG_ARM_GIC_V2)	+= ../arm/gic-v2.o
