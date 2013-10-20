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

jailhouse: jailhouse.c ../jailhouse.h ../hypervisor/include/jailhouse/cell-config.h
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f jailhouse
