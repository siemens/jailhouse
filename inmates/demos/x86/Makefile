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

include $(INMATES_LIB)/Makefile.lib

LIBDIR := ../../lib/x86

INMATES := tiny-demo.bin apic-demo.bin

tiny-demo-y := tiny-demo.o \
	$(LIBDIR)/header.o $(LIBDIR)/printk.o $(LIBDIR)/pm-timer.o

apic-demo-y := apic-demo.o \
	$(LIBDIR)/header.o $(LIBDIR)/printk.o $(LIBDIR)/pm-timer.o

$(eval $(call DECLARE_TARGETS,$(INMATES)))
