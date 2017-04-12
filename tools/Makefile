#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013-2017
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

# includes installation-related variables and definitions
include $(src)/../scripts/include.mk

LD = $(CC)
NOSTDINC_FLAGS :=
LINUXINCLUDE := -I$(src)/../driver
KBUILD_CFLAGS := -g -O3 -DLIBEXECDIR=\"$(libexecdir)\" \
	-Wall -Wextra -Wmissing-declarations -Wmissing-prototypes -Werror \
	-DJAILHOUSE_VERSION=\"$(shell cat $(src)/../VERSION)\" $(EXTRA_CFLAGS)
LDFLAGS :=
GCOV_PROFILE := n

BINARIES := jailhouse

HELPERS := \
	jailhouse-cell-linux \
	jailhouse-cell-stats \
	jailhouse-config-create \
	jailhouse-hardware-check
TEMPLATES := jailhouse-config-collect.tmpl root-cell-config.c.tmpl

always := $(BINARIES)

HAS_PYTHON_MAKO := \
	$(shell python -c "from mako.template import Template" 2>/dev/null \
	&& echo yes)

ifeq ($(strip $(HAS_PYTHON_MAKO)), yes)
	always += jailhouse-config-collect
	HELPERS += jailhouse-config-collect
else
	always += no_python_mako
endif

define patch_dirvar
	sed -i 's|^$1 = None|$1 = "$($1)"|' $2
endef

quiet_cmd_gen_collect = GEN     $@
define cmd_gen_collect
	$< -g $@; \
	chmod +x $@
endef

targets += jailhouse.o

$(obj)/jailhouse: $(obj)/jailhouse.o
	$(call if_changed,ld)

CFLAGS_jailhouse-gcov-extract.o	:= -I$(src)/../hypervisor/include \
	-I$(src)/../hypervisor/arch/$(SRCARCH)/include
# just change ldflags not cflags, we are not profiling the tool
LDFLAGS_jailhouse-gcov-extract := -lgcov -fprofile-arcs

targets += jailhouse-gcov-extract.o
always += jailhouse-gcov-extract

$(obj)/jailhouse-gcov-extract: $(obj)/jailhouse-gcov-extract.o
	$(call if_changed,ld)

$(obj)/jailhouse-config-collect: $(src)/jailhouse-config-create $(src)/jailhouse-config-collect.tmpl
	$(call if_changed,gen_collect)

install-bin: $(BINARIES) $(DESTDIR)$(sbindir)
	$(INSTALL_PROGRAM) $^

install-libexec: $(HELPERS) $(DESTDIR)$(libexecdir)/jailhouse
	$(INSTALL_PROGRAM) $^
	$(Q)$(call patch_dirvar,libexecdir,$(lastword $^)/jailhouse-cell-linux)
	$(Q)$(call patch_dirvar,datadir,$(lastword $^)/jailhouse-config-create)

install-data: $(TEMPLATES) $(DESTDIR)$(datadir)/jailhouse
	$(INSTALL_DATA) $^

install-completion: jailhouse-completion.bash $(DESTDIR)$(completionsdir)
	$(INSTALL_DATA) $< $(DESTDIR)$(completionsdir)/jailhouse

install: install-bin install-libexec install-data install-completion

$(obj)/no_python_mako:
	@echo -ne "WARNING: Could not create the helper script to generate" \
		  "configurations on remote machines" \
		  "(\"jailhouse-conf-collect\"). You need Python and the" \
		  "Mako library for it.\n"

.PHONY: install install-bin install-libexec install-data install-completion \
	no_python_mako
