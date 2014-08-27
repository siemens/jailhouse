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

LINUXINCLUDE := -I$(src)/arch/$(SRCARCH)/include -I$(src)/include
KBUILD_CFLAGS := -g -Os -Wall -Wstrict-prototypes -Wtype-limits \
		 -Wmissing-declarations -Wmissing-prototypes \
		 -fno-strict-aliasing -fno-pic -fno-common \
		 -fno-stack-protector -fno-builtin-ffsl

ifeq ($(SRCARCH),x86)
KBUILD_CFLAGS += -mcmodel=kernel
KBUILD_CPPFLAGS += -m64
endif

ifneq ($(wildcard $(src)/include/jailhouse/config.h),)
KBUILD_CFLAGS += -include $(src)/include/jailhouse/config.h
endif

$(obj)/arch/$(SRCARCH)/built-in.o: FORCE
	$(Q)$(MAKE) $(build)=$(obj)/arch/$(SRCARCH)

always := jailhouse.bin

hypervisor-y := setup.o printk.o paging.o control.o lib.o \
	arch/$(SRCARCH)/built-in.o hypervisor.lds
targets += $(hypervisor-y)

define filechk_version
	$(src)/../scripts/gen_version_h $(src)/..
endef

clean-files := $(obj)/include/jailhouse/version.h

$(obj)/include/jailhouse/version.h: $(src)/Makefile FORCE
	$(call filechk,version)

$(obj)/setup.o: $(obj)/include/jailhouse/version.h

HYPERVISOR_OBJS = $(addprefix $(obj)/,$(hypervisor-y))

LDFLAGS_hypervisor.o := -T

targets += hypervisor.o
$(obj)/hypervisor.o: $(src)/hypervisor.lds $(HYPERVISOR_OBJS)
	$(call if_changed,ld)

OBJCOPYFLAGS_jailhouse.bin := -O binary -R .eh_frame

targets += jailhouse.bin
$(obj)/jailhouse.bin: $(obj)/hypervisor.o
	$(call if_changed,objcopy)
