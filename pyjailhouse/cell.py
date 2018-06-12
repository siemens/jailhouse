
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015-2016
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.

import ctypes
import errno
import fcntl
import struct


class JailhouseCell:
    JAILHOUSE_CELL_CREATE = 0x40100002
    JAILHOUSE_CELL_LOAD = 0x40300003
    JAILHOUSE_CELL_START = 0x40280004

    JAILHOUSE_CELL_ID_UNUSED = -1

    def __init__(self, config):
        self.name = config.name.encode()

        self.dev = open('/dev/jailhouse')

        cbuf = ctypes.c_buffer(config.data)
        create = struct.pack('QI4x', ctypes.addressof(cbuf), len(config.data))
        try:
            fcntl.ioctl(self.dev, JailhouseCell.JAILHOUSE_CELL_CREATE, create)
        except IOError as e:
            if e.errno != errno.EEXIST:
                raise e

    def load(self, image, address):
        cbuf = ctypes.create_string_buffer(bytes(image))

        load = struct.pack('i4x32sI4xQQQ8x',
                           JailhouseCell.JAILHOUSE_CELL_ID_UNUSED, self.name,
                           1, ctypes.addressof(cbuf), len(image), address)
        fcntl.ioctl(self.dev, self.JAILHOUSE_CELL_LOAD, load)

    def start(self):
        start = struct.pack('i4x32s', JailhouseCell.JAILHOUSE_CELL_ID_UNUSED,
                            self.name)
        fcntl.ioctl(self.dev, JailhouseCell.JAILHOUSE_CELL_START, start)
