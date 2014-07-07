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

CC = $(CROSS_COMPILE)gcc

CFLAGS = -g -O3 -I.. -I../hypervisor/include \
	-Wall -Wmissing-declarations -Wmissing-prototypes

TARGETS := jailhouse

HAS_PYTHON_MAKO := \
	$(shell python -c "from mako.template import Template" 2>/dev/null \
	&& echo yes)

ifeq ($(strip $(HAS_PYTHON_MAKO)), yes)
	TARGETS += jailhouse-config-collect
else
	TARGETS += no_python_mako
endif

all: $(TARGETS)

jailhouse: jailhouse.c ../jailhouse.h ../hypervisor/include/jailhouse/cell-config.h
	$(CC) $(CFLAGS) -o $@ $<

jailhouse-config-collect: jailhouse-config-create jailhouse-config-collect.tmpl
	./$< -g $@
	chmod +x $@

.PHONY: clean no_python_mako

clean:
	rm -f $(TARGETS)

no_python_mako:
	@echo -ne "WARNING: Could not create the helper script to generate" \
		  "configurations on remote machines" \
		  "(\"jailhouse-conf-collect\"). You need Python and the" \
		  "Mako library for it.\n"
