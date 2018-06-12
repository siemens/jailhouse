#
# pyjailhouse, a python interface for the Jailhouse hypervisor
#
# Copyright (c) Christopher Goldsworthy, 2018
#
# This script is used to create project metadata when installing pyjailhouse
# using pip.
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

from setuptools import setup, find_packages

with open("VERSION") as version_file:
    version = version_file.read().lstrip("v")

setup(name="pyjailhouse", version=version,
      description="A Python interface for the Jailhouse Hypervisor",
      license="GPLv2", url="https://github.com/siemens/jailhouse",
      author_email="jailhouse-dev@googlegroups.com",
      packages=find_packages())
