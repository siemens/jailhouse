#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013
# Copyright (c) Valentine Sinitsyn, 2014
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

BUILT_IN_OBJECTS := built-in-amd.o built-in-intel.o
COMMON_OBJECTS := apic.o dbg-write.o entry.o setup.o control.o mmio.o iommu.o \
		  paging.o ../../pci.o pci.o ioapic.o i8042.o vcpu.o \
		  ../../pci_ivshmem.o

always := $(BUILT_IN_OBJECTS)

obj-y := $(BUILT_IN_OBJECTS)

built-in-amd-y := $(COMMON_OBJECTS) svm.o amd_iommu.o svm-vmexit.o cat-stubs.o
built-in-intel-y := $(COMMON_OBJECTS) vmx.o vtd.o vmx-vmexit.o cat.o
