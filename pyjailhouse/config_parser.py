#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015-2020
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# This script should help to create a basic jailhouse configuration file.
# It needs to be executed on the target machine, where it will gather
# information about the system. For more advanced scenarios you will have
# to change the generated C-code.

from __future__ import print_function
import struct

from .extendedenum import ExtendedEnum

# Keep the whole file in sync with include/jailhouse/cell-config.h.
_CONFIG_REVISION = 13


def flag_str(enum_class, value, separator=' | '):
    flags = []
    while value:
        mask = 1 << (value.bit_length() - 1)
        flags.insert(0, str(enum_class(mask)))
        value &= ~mask
    return separator.join(flags)


class JAILHOUSE_MEM(ExtendedEnum, int):
    _ids = {
        'READ':         0x00001,
        'WRITE':        0x00002,
        'EXECUTE':      0x00004,
        'DMA':          0x00008,
        'IO':           0x00010,
        'COMM_REGION':  0x00020,
        'LOADABLE':     0x00040,
        'ROOTSHARED':   0x00080,
        'NO_HUGEPAGES': 0x00100,
        'IO_UNALIGNED': 0x08000,
        'IO_8':         0x10000,
        'IO_16':        0x20000,
        'IO_32':        0x40000,
        'IO_64':        0x80000,
    }


class MemRegion:
    _REGION_FORMAT = 'QQQQ'
    SIZE = struct.calcsize(_REGION_FORMAT)

    def __init__(self, region_struct):
        (self.phys_start,
         self.virt_start,
         self.size,
         self.flags) = \
            struct.unpack_from(MemRegion._REGION_FORMAT, region_struct)

    def __str__(self):
        return ("  phys_start: 0x%016x\n" % self.phys_start) + \
               ("  virt_start: 0x%016x\n" % self.virt_start) + \
               ("  size:       0x%016x\n" % self.size) + \
               ("  flags:      " + flag_str(JAILHOUSE_MEM, self.flags))

    def is_ram(self):
        return ((self.flags & (JAILHOUSE_MEM.READ |
                               JAILHOUSE_MEM.WRITE |
                               JAILHOUSE_MEM.EXECUTE |
                               JAILHOUSE_MEM.DMA |
                               JAILHOUSE_MEM.IO |
                               JAILHOUSE_MEM.COMM_REGION |
                               JAILHOUSE_MEM.ROOTSHARED)) ==
                (JAILHOUSE_MEM.READ |
                 JAILHOUSE_MEM.WRITE |
                 JAILHOUSE_MEM.EXECUTE |
                 JAILHOUSE_MEM.DMA))

    def is_comm_region(self):
        return (self.flags & JAILHOUSE_MEM.COMM_REGION) != 0

    def phys_address_in_region(self, address):
        return address >= self.phys_start and \
            address < (self.phys_start + self.size)

    def phys_overlaps(self, region):
        if self.size == 0 or region.size == 0:
            return False
        return region.phys_address_in_region(self.phys_start) or \
            self.phys_address_in_region(region.phys_start)

    def virt_address_in_region(self, address):
        return address >= self.virt_start and \
            address < (self.virt_start + self.size)

    def virt_overlaps(self, region):
        if self.size == 0 or region.size == 0:
            return False
        return region.virt_address_in_region(self.virt_start) or \
            self.virt_address_in_region(region.virt_start)


class CacheRegion:
    _REGION_FORMAT = 'IIBxH'
    SIZE = struct.calcsize(_REGION_FORMAT)


class Irqchip:
    _IRQCHIP_FORMAT = 'QIIQQ'
    SIZE = struct.calcsize(_IRQCHIP_FORMAT)

    def __init__(self, irqchip_struct):
        (self.address,
         self.id,
         self.pin_base,
         self.pin_bitmap_lo,
         self.pin_bitmap_hi) = \
            struct.unpack_from(self._IRQCHIP_FORMAT, irqchip_struct)

    def is_standard(self):
        return self.address == 0xfec00000


class PIORegion:
    _REGION_FORMAT = 'HH'
    SIZE = struct.calcsize(_REGION_FORMAT)

    def __init__(self, pio_struct):
        (self.base, self.length) = struct.unpack_from(self._REGION_FORMAT,
                                                      pio_struct)


class CellConfig:
    _HEADER_FORMAT = '=6sH32s4xIIIIIIIIIIQ8x32x'

    def __init__(self, data, root_cell=False):
        self.data = data

        try:
            (signature,
             revision,
             name,
             self.flags,
             self.cpu_set_size,
             self.num_memory_regions,
             self.num_cache_regions,
             self.num_irqchips,
             self.num_pio_regions,
             self.num_pci_devices,
             self.num_pci_caps,
             self.num_stream_ids,
             self.vpci_irq_base,
             self.cpu_reset_address) = \
                struct.unpack_from(CellConfig._HEADER_FORMAT, self.data)
            if not root_cell:
                if signature != b'JHCELL':
                    raise RuntimeError('Not a cell configuration')
                if revision != _CONFIG_REVISION:
                    raise RuntimeError('Configuration file revision mismatch')
            self.name = str(name.decode().strip('\0'))

            mem_region_offs = struct.calcsize(CellConfig._HEADER_FORMAT) + \
                self.cpu_set_size
            self.memory_regions = []
            for n in range(self.num_memory_regions):
                self.memory_regions.append(
                    MemRegion(self.data[mem_region_offs:]))
                mem_region_offs += MemRegion.SIZE

            irqchip_offs = mem_region_offs + \
                self.num_cache_regions * CacheRegion.SIZE
            self.irqchips = []
            for n in range(self.num_irqchips):
                self.irqchips.append(
                    Irqchip(self.data[irqchip_offs:]))
                irqchip_offs += Irqchip.SIZE

            pioregion_offs = irqchip_offs
            self.pio_regions = []
            for n in range(self.num_pio_regions):
                self.pio_regions.append(PIORegion(self.data[pioregion_offs:]))
                pioregion_offs += PIORegion.SIZE
        except struct.error:
            raise RuntimeError('Not a %scell configuration' %
                               ('root ' if root_cell else ''))


class SystemConfig:
    _HEADER_FORMAT = '=6sH4x'
    # ...followed by MemRegion as hypervisor memory
    _CONSOLE_AND_PLATFORM_FORMAT = '32x12x224x44x'

    def __init__(self, data):
        self.data = data

        try:
            (signature,
             revision) = \
                struct.unpack_from(SystemConfig._HEADER_FORMAT, self.data)

            if signature != b'JHSYST':
                raise RuntimeError('Not a root cell configuration')
            if revision != _CONFIG_REVISION:
                raise RuntimeError('Configuration file revision mismatch')

            offs = struct.calcsize(SystemConfig._HEADER_FORMAT)
            self.hypervisor_memory = MemRegion(self.data[offs:])

            offs += struct.calcsize(MemRegion._REGION_FORMAT)
            offs += struct.calcsize(SystemConfig._CONSOLE_AND_PLATFORM_FORMAT)
        except struct.error:
            raise RuntimeError('Not a root cell configuration')

        self.root_cell = CellConfig(self.data[offs:], root_cell=True)
