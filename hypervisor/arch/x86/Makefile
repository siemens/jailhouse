#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013-2016
# Copyright (c) Valentine Sinitsyn, 2014
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

KBUILD_CFLAGS += -mcmodel=kernel -mno-sse -mno-mmx -mno-sse2 -mno-3dnow
KBUILD_CFLAGS += -mno-red-zone
KBUILD_CFLAGS += $(call cc-option,-mno-avx,)

KBUILD_CPPFLAGS += -m64

BUILD_VARIANTS := amd intel
