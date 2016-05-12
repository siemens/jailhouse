#!/bin/bash
#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015, 2016
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

set -e

CONFIGS="x86 banana-pi vexpress amd-seattle"

# only build a specific config if the branch selects it
if [ ${TRAVIS_BRANCH#coverity_scan-} != ${TRAVIS_BRANCH} ]; then
	CONFIGS=${TRAVIS_BRANCH#coverity_scan-}
fi

PREFIX=
if [ "$1" == "--cov" ]; then
	export COVERITY_UNSUPPORTED=1
	PREFIX="cov-build --append-log --dir $2 $3"
fi

for CONFIG in $CONFIGS; do
	echo
	echo "*** Building configuration $CONFIG ***"

	cp ci/jailhouse-config-$CONFIG.h hypervisor/include/jailhouse/config.h

	case $CONFIG in
	x86)
		ARCH=x86_64
		CROSS_COMPILE=
		;;
	amd-seattle)
		ARCH=arm64
		CROSS_COMPILE=aarch64-linux-gnu-
		;;
	*)
		ARCH=arm
		CROSS_COMPILE=arm-linux-gnueabihf-
		;;
	esac

	$PREFIX make KDIR=ci/linux/build-$CONFIG ARCH=$ARCH \
	     CROSS_COMPILE=$CROSS_COMPILE

	# Keep the clean run out of sight for cov-build so that results are
	# accumulated as far as possible. Multiple compilations of the same
	# file will still leave only the last run in the results.
	make KDIR=ci/linux/build-$CONFIG ARCH=$ARCH \
	     CROSS_COMPILE=$CROSS_COMPILE clean
done
