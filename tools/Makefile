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

prefix      ?= /usr/local
exec_prefix ?= $(prefix)
bindir      ?= $(exec_prefix)/bin
libexecdir  ?= $(exec_prefix)/libexec
datarootdir ?= $(prefix)/share
datadir     ?= $(datarootdir)

ifeq ($(V),1)
	Q =
else
	Q = @
endif

INSTALL         ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA    ?= $(INSTALL) -m 644
INSTALL_DIR     ?= $(INSTALL) -d -m 755

CC = $(CROSS_COMPILE)gcc

CFLAGS = -g -O3 -I.. -I../hypervisor/include -DLIBEXECDIR=\"$(libexecdir)\" \
	-Wall -Wmissing-declarations -Wmissing-prototypes

TARGETS := jailhouse

INST_TARGETS := $(TARGETS)
HELPERS := \
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

define patch_datadir_var
	sed -i 's|^datadir = None|datadir = "$(datadir)"|' $1
endef

all: $(TARGETS)

jailhouse: jailhouse.c ../jailhouse.h ../hypervisor/include/jailhouse/cell-config.h
	$(CC) $(CFLAGS) -o $@ $<

jailhouse-config-collect: jailhouse-config-create jailhouse-config-collect.tmpl
	./$< -g $@
	$(Q)chmod +x $@

install-bin: $(INST_TARGETS)
	$(INSTALL_DIR) $(DESTDIR)$(bindir)
	$(INSTALL_PROGRAM) $^ $(DESTDIR)$(bindir)

install-libexec: $(HELPERS)
	$(INSTALL_DIR) $(DESTDIR)$(libexecdir)/jailhouse
	$(INSTALL_PROGRAM) $^ $(DESTDIR)$(libexecdir)/jailhouse
	$(Q)$(call patch_datadir_var, \
	      $(DESTDIR)$(libexecdir)/jailhouse/jailhouse-config-create)

install-data: $(TEMPLATES)
	$(INSTALL_DIR) $(DESTDIR)$(datadir)/jailhouse
	$(INSTALL_DATA) $^ $(DESTDIR)$(datadir)/jailhouse

install: install-bin install-libexec install-data

clean:
	rm -f $(TARGETS)

no_python_mako:
	@echo -ne "WARNING: Could not create the helper script to generate" \
		  "configurations on remote machines" \
		  "(\"jailhouse-conf-collect\"). You need Python and the" \
		  "Mako library for it.\n"

.PHONY: install install-bin install-libexec install-data clean no_python_mako
