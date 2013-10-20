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

ccflags-y := -I$(src)/../hypervisor/include

OBJCOPYFLAGS := -O binary

CONFIGS = $(shell cd $(src); ls *.c)

always := $(CONFIGS:.c=.cell)

targets += $(CONFIGS:.c=.o) $(CONFIGS:.c=.cell)

dummy: $(addprefix $(obj)/,$(CONFIGS:.c=.o))

$(obj)/%.cell: $(obj)/%.o
	$(call if_changed,objcopy)
