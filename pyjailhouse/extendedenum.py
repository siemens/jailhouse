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

# Python 2 and 3 have different ways of handling metaclasses. This decorator
# is a support layer for both and can be removed once Python 2 is no longer
# supported.
def with_metaclass(meta):
    def decorator(cls):
        body = vars(cls).copy()
        body.pop('__dict__', None)
        body.pop('__weakref__', None)
        return meta(cls.__name__, cls.__bases__, body)
    return decorator


class ExtendedEnumMeta(type):
    def __getattr__(cls, key):
        return cls(cls._ids[key])


@with_metaclass(ExtendedEnumMeta)
class ExtendedEnum:
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
