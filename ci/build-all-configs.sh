#!/bin/bash
#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

CONFIGS="x86 banana-pi vexpress"

for CONFIG in $CONFIGS; do
	echo
	echo "*** Building configuration $CONFIG ***"

	cp ci/jailhouse-config-$CONFIG.h hypervisor/include/jailhouse/config.h

	case $CONFIG in
	x86)
		ARCH=x86_64
		CROSS_COMPILE=
		;;
	*)
		ARCH=arm
		CROSS_COMPILE=arm-linux-gnueabihf-
		;;
	esac

	make KDIR=ci/linux/build-$CONFIG ARCH=$ARCH \
	     CROSS_COMPILE=$CROSS_COMPILE clean all
done
