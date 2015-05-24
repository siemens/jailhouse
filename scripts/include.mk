#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2014
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Benjamin Block <bebl@mageta.org>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

ifeq ($(V),1)
	Q =
else
	Q = @
endif

MAKEFLAGS += --no-print-directory

prefix      ?= /usr/local
exec_prefix ?= $(prefix)
sbindir     ?= $(exec_prefix)/sbin
libexecdir  ?= $(exec_prefix)/libexec
datarootdir ?= $(prefix)/share
datadir     ?= $(datarootdir)
firmwaredir ?= /lib/firmware

# all directories listed here will be created using a generic rule below
INSTALL_DIRECTORIES := $(prefix)	\
		       $(exec_prefix)	\
		       $(sbindir)	\
		       $(libexecdir)	\
		       $(datarootdir)	\
		       $(datadir)	\
		       $(firmwaredir)

INSTALL         ?= install
INSTALL_PROGRAM ?= $(INSTALL)
INSTALL_DATA    ?= $(INSTALL) -m 644
INSTALL_DIR     ?= $(INSTALL) -d -m 755

# creates a rule for each dir in $(INSTALL_DIRECTORIES) under the current
# $(DESTDIR) and additionally to that for each of these dirs a subdir named
# `jailhouse`. These can be used as prerequirement for install-rules and will
# thus be created on demand (or not at all if not used in that way).
$(sort $(INSTALL_DIRECTORIES:%=$(DESTDIR)%) \
	$(INSTALL_DIRECTORIES:%=$(DESTDIR)%/jailhouse)):
	$(INSTALL_DIR) $@

ARCH ?= $(shell uname -m)
ifeq ($(ARCH),x86_64)
	ARCH = x86
endif
