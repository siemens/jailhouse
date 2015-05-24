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

CC = $(CROSS_COMPILE)gcc

CFLAGS = -g -O3 -I../driver -DLIBEXECDIR=\"$(libexecdir)\" \
	-Wall -Wmissing-declarations -Wmissing-prototypes \
	-DJAILHOUSE_VERSION=\"$(shell cat ../VERSION)\"

TARGETS := jailhouse

INST_TARGETS := $(TARGETS)
HELPERS := \
	jailhouse-cell-linux \
	jailhouse-cell-list \
	jailhouse-cell-stats \
	jailhouse-config-create
TEMPLATES := jailhouse-config-collect.tmpl root-cell-config.c.tmpl

HAS_PYTHON_MAKO := \
	$(shell python -c "from mako.template import Template" 2>/dev/null \
	&& echo yes)

ifeq ($(strip $(HAS_PYTHON_MAKO)), yes)
	TARGETS += jailhouse-config-collect
	HELPERS += jailhouse-config-collect
else
	TARGETS += no_python_mako
endif

define patch_dirvar
	sed -i 's|^$1 = None|$1 = "$($1)"|' $2
endef

# no recipes above this one (also no includes)
all: $(TARGETS)

# includes installation-related variables and definitions
include ../scripts/include.mk

jailhouse: jailhouse.c ../driver/jailhouse.h ../VERSION
	$(CC) $(CFLAGS) -o $@ $<

jailhouse-config-collect: jailhouse-config-create jailhouse-config-collect.tmpl
	./$< -g $@
	$(Q)chmod +x $@

install-bin: $(INST_TARGETS) $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) $^

install-libexec: $(HELPERS) $(DESTDIR)$(libexecdir)/jailhouse
	$(INSTALL_PROGRAM) $^
	$(Q)$(call patch_dirvar,libexecdir,$(lastword $^)/jailhouse-cell-linux)
	$(Q)$(call patch_dirvar,datadir,$(lastword $^)/jailhouse-config-create)

install-data: $(TEMPLATES) $(DESTDIR)$(datadir)/jailhouse
	$(INSTALL_DATA) $^

install: install-bin install-libexec install-data

clean:
	rm -f $(TARGETS)

no_python_mako:
	@echo -ne "WARNING: Could not create the helper script to generate" \
		  "configurations on remote machines" \
		  "(\"jailhouse-conf-collect\"). You need Python and the" \
		  "Mako library for it.\n"

.PHONY: install install-bin install-libexec install-data clean no_python_mako
