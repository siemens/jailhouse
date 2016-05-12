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

INMATES_LIB = $(src)/lib/$(SRCARCH)
export INMATES_LIB

INCLUDES := -I$(INMATES_LIB) \
	    -I$(src)/../hypervisor/arch/$(SRCARCH)/include \
	    -I$(src)/../hypervisor/include

ifeq ($(SRCARCH),arm64)
INCLUDES += -I$(src)/../hypervisor/arch/arm/include
endif

LINUXINCLUDE  :=
KBUILD_AFLAGS += $(INCLUDES)
KBUILD_CFLAGS := -g -Os -Wall -Wstrict-prototypes -Wtype-limits \
		 -Wmissing-declarations -Wmissing-prototypes \
		 -fno-strict-aliasing -fomit-frame-pointer -fno-pic \
		 -fno-common -fno-stack-protector $(INCLUDES)
ifneq ($(wildcard $(src)/../hypervisor/include/jailhouse/config.h),)
KBUILD_CFLAGS += -include $(src)/../hypervisor/include/jailhouse/config.h
endif

OBJCOPYFLAGS := -O binary
LDFLAGS += --gc-sections -T

subdir-y := lib/$(SRCARCH) demos/$(SRCARCH) tools/$(SRCARCH)

# demos and tools depend on the library
$(obj)/demos/$(SRCARCH) $(obj)/tools/$(SRCARCH): $(obj)/lib/$(SRCARCH)
