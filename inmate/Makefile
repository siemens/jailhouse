#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

LINUXINCLUDE := -I$(src)
KBUILD_CFLAGS := -g -Os -Wall -Wstrict-prototypes -Wtype-limits \
		 -Wmissing-declarations -Wmissing-prototypes \
		 -fno-strict-aliasing -fomit-frame-pointer -fno-pic \
		 -fno-common -fno-stack-protector -I.
ifneq ($(wildcard $(src)/../hypervisor/include/jailhouse/config.h),)
KBUILD_CFLAGS += -include $(src)/../hypervisor/include/jailhouse/config.h
endif

OBJCOPYFLAGS := -O binary
LDFLAGS := -T

ifeq ($(SRCARCH), x86)
always := tiny-demo.bin apic-demo.bin
endif

tiny-demo-y := tiny-demo.o header.o printk.o pm-timer.o
targets += $(tiny-demo-y)

TINY_DEMO_OBJS = $(addprefix $(obj)/,$(tiny-demo-y))

target += tiny-demo-linked.o
$(obj)/tiny-demo-linked.o: $(src)/inmate.lds $(TINY_DEMO_OBJS)
	$(call if_changed,ld)


apic-demo-y := apic-demo.o header.o printk.o pm-timer.o
targets += $(apic-demo-y)

APIC_DEMO_OBJS = $(addprefix $(obj)/,$(apic-demo-y))

target += apic-demo-linked.o
$(obj)/apic-demo-linked.o: $(src)/inmate.lds $(APIC_DEMO_OBJS)
	$(call if_changed,ld)


targets += tiny-demo.bin apic-demo.bin
$(obj)/%.bin: $(obj)/%-linked.o
	$(call if_changed,objcopy)
