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

lib-y				:= header.o gic.o printk.o timer.o
lib-$(CONFIG_ARM_GIC)		+= gic-v2.o
lib-$(CONFIG_ARM_GIC_V3)	+= gic-v3.o
lib-$(CONFIG_SERIAL_AMBA_PL011)	+= uart-pl011.o
lib-$(CONFIG_SERIAL_8250_DW)	+= uart-8250-dw.o
lib-$(CONFIG_SERIAL_TEGRA)	+= uart-tegra.o
