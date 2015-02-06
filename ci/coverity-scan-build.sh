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

curl -s https://scan.coverity.com/scripts/travisci_build_coverity_scan.sh \
	-o ci/travisci_build_coverity_scan.sh.orig

# Patch the line that starts the build.
# We need to control this step via our build script.
sed 's/^COVERITY_UNSUPPORTED=1 cov-build --dir.*/ci\/build-all-configs.sh --cov \$RESULTS_DIR \$COV_BUILD_OPTIONS/' \
	ci/travisci_build_coverity_scan.sh.orig > ci/travisci_build_coverity_scan.sh.step1

# Path the branch name into the description.
sed 's/^  --form description=.*/  --form description="Travis CI build (branch: \$TRAVIS_BRANCH)" \\/' \
	ci/travisci_build_coverity_scan.sh.step1 > ci/travisci_build_coverity_scan.sh

# Check if the patch applied, bail out if not.
if diff -q ci/travisci_build_coverity_scan.sh.orig \
	   ci/travisci_build_coverity_scan.sh.step1 > /dev/null || \
   diff -q ci/travisci_build_coverity_scan.sh.step1 \
	   ci/travisci_build_coverity_scan.sh > /dev/null; then
	echo "Unable to patch Coverity script!"
	exit 1
fi

# Run the patched scanner script.
. ci/travisci_build_coverity_scan.sh
