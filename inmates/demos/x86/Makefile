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

INMATES := tiny-demo.bin apic-demo.bin ioapic-demo.bin 32-bit-demo.bin \
	pci-demo.bin e1000-demo.bin ivshmem-demo.bin smp-demo.bin \
	cache-timings.bin

tiny-demo-y	:= tiny-demo.o
apic-demo-y	:= apic-demo.o
ioapic-demo-y	:= ioapic-demo.o
pci-demo-y	:= pci-demo.o
e1000-demo-y	:= e1000-demo.o
ivshmem-demo-y	:= ../ivshmem-demo.o
smp-demo-y	:= smp-demo.o
cache-timings-y := cache-timings.o

$(eval $(call DECLARE_32_BIT,32-bit-demo))
32-bit-demo-y	:= 32-bit-demo.o

$(eval $(call DECLARE_TARGETS,$(INMATES)))
