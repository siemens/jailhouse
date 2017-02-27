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

always := lib.a lib32.a

TARGETS := header.o hypercall.o ioapic.o printk.o smp.o
TARGETS += ../pci.o ../string.o ../cmdline.o
TARGETS_64_ONLY := int.o mem.o pci.o timing.o

ccflags-y := -ffunction-sections

lib-y := $(TARGETS) $(TARGETS_64_ONLY)

lib32-y := $(addprefix $(obj)/,$(TARGETS:.o=-32.o))

quiet_cmd_link_archive32 = AR      $@
cmd_link_archive32 = rm -f $@; $(AR) rcs$(KBUILD_ARFLAGS) $@ $(lib32-y)

$(obj)/lib32.a: $(lib32-y)
	$(call if_changed,link_archive32)

AFLAGS_header-32.o := -m32
targets += header-32.o

$(obj)/%-32.o: c_flags += -m32
$(obj)/%-32.o: $(src)/%.c
	$(call if_changed_rule,cc_o_c)
