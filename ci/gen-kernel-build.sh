#!/bin/bash
#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2014-2016
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

BASEDIR=`cd \`dirname $0\`; pwd`

if test -z $KERNEL; then
	KERNEL=https://www.kernel.org/pub/linux/kernel/v4.x/linux-4.4.tar.xz
fi
if test -z $PARALLEL_BUILD; then
	PARALLEL_BUILD=-j16
fi
if test -z $OUTDIR; then
	OUTDIR=$BASEDIR/out
fi

prepare_out()
{
	rm -rf $OUTDIR
	mkdir -p $OUTDIR
	cd $OUTDIR
}

prepare_kernel()
{
	ARCHIVE_FILE=`basename $KERNEL`
	if ! test -f $BASEDIR/$ARCHIVE_FILE; then
		wget $KERNEL -O $BASEDIR/$ARCHIVE_FILE
	fi
	tar xJf $BASEDIR/$ARCHIVE_FILE
	ln -s linux-* linux
	cd linux
}

build_kernel()
{
	mkdir build-$1
	cp $BASEDIR/kernel-config-$1 build-$1/.config
	make O=build-$1 vmlinux $PARALLEL_BUILD ARCH=$2 CROSS_COMPILE=$3
	# clean up some unneeded build output
	find build-$1 \( -name "*.o" -o -name "*.cmd" -o -name ".tmp_*" \) -exec rm -rf {} \;
}

package_out()
{
	cd $OUTDIR
	tar cJf kernel-build.tar.xz linux-* linux
}

prepare_out
prepare_kernel
build_kernel x86 x86_64
build_kernel banana-pi arm arm-linux-gnueabihf-
build_kernel vexpress arm arm-linux-gnueabihf-
build_kernel amd-seattle arm64 aarch64-linux-gnu-
package_out
