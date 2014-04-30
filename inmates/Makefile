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
		 -fno-common -fno-stack-protector -I. \
		 -I$(src)/../hypervisor/arch/$(SRCARCH)/include \
		 -I$(src)/../hypervisor/include
ifneq ($(wildcard $(src)/../hypervisor/include/jailhouse/config.h),)
KBUILD_CFLAGS += -include $(src)/../hypervisor/include/jailhouse/config.h
endif

OBJCOPYFLAGS := -O binary
LDFLAGS += -T


ifeq ($(SRCARCH), x86)
KBUILD_CFLAGS += -m64
INMATES := tiny-demo.bin apic-demo.bin
endif

tiny-demo-y := tiny-demo.o header.o printk.o pm-timer.o

apic-demo-y := apic-demo.o header.o printk.o pm-timer.o


always := $(INMATES)

# $(NAME-y) NAME-linked.o NAME.bin
targets += $(foreach t,$(INMATES:.bin=-y),$($t)) $(INMATES:.bin=-linked.o) \
		$(INMATES)

# prevent deleting intermediate files which would cause rebuilds
.SECONDARY: $(addprefix $(obj)/,$(targets))

# obj/NAME-linked.o: ... obj/$(NAME-y)
.SECONDEXPANSION:
$(obj)/%-linked.o: $(src)/inmate.lds $$(addprefix $$(obj)/,$$($$*-y))
	$(call if_changed,ld)

$(obj)/%.bin: $(obj)/%-linked.o
	$(call if_changed,objcopy)
