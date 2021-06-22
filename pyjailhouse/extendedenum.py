#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) OTH Regensburg, 2019
#
# Authors:
#  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.

class ExtendedEnumMeta(type):
    def __getattr__(cls, key):
        return cls(cls._ids[key])


class ExtendedEnum(metaclass=ExtendedEnumMeta):
    def __init__(self, value):
        self.value = value

    def __str__(self):
        for key, value in self._ids.items():
            if value == self.value:
                return '%s_%s' % (self.__class__.__name__, key)

        return '0x%x' % self.value

    def __eq__(self, other):
        if isinstance(other, self.__class__):
            return self.value == other.value
        elif isinstance(other, int):
            return self.value == other
        return False
