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

import struct

from .extendedenum import ExtendedEnum

# Keep the whole file in sync with include/jailhouse/cell-config.h.
_CONFIG_REVISION = 14
JAILHOUSE_X86 = 0
JAILHOUSE_ARM = 1
JAILHOUSE_ARM64 = 2

JAILHOUSE_ARCH_MAX = 2


def convert_arch(arch):
    if arch > JAILHOUSE_ARCH_MAX:
        raise RuntimeError('Configuration has unsupported architecture')
    return {
        JAILHOUSE_X86: 'x86',
        JAILHOUSE_ARM: 'arm',
        JAILHOUSE_ARM64: 'arm64',
    }[arch]


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
    _HEADER_FORMAT = '=5sBH32s4xIIIIIIIIIIQ8x32x'

    def __init__(self, data, root_cell=False):
        self.data = data

        try:
            (signature,
             self.arch,
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
                if signature != b'JHCLL':
                    raise RuntimeError('Not a cell configuration')
                if revision != _CONFIG_REVISION:
                    raise RuntimeError('Configuration file revision mismatch')
            self.name = str(name.decode().strip('\0'))
            self.arch = convert_arch(self.arch)

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


class JAILHOUSE_IOMMU(ExtendedEnum, int):
    _ids = {
        'UNUSED':     0,
        'AMD':        1,
        'INTEL':      2,
        'SMMUV3':     3,
        'TIPVU':      4,
        'ARM_MMU500': 5,
    }


class IOMMU:
    _IOMMU_HEADER_FORMAT = '=IQI'
    _IOMMU_AMD_FORMAT = '=HBBI2x'
    _IOMMU_TIPVU_FORMAT = '=QI'
    _IOMMU_OTHER_FORMAT = '12x'
    SIZE = struct.calcsize(_IOMMU_HEADER_FORMAT + _IOMMU_OTHER_FORMAT)

    def __init__(self, iommu_struct):
        (self.type,
         self.base,
         self.size) = \
            struct.unpack_from(self._IOMMU_HEADER_FORMAT, iommu_struct)

        offs = struct.calcsize(self._IOMMU_HEADER_FORMAT)
        if self.type == JAILHOUSE_IOMMU.AMD:
            (self.amd_bdf,
             self.amd_base_cap,
             self.amd_msi_cap,
             self.amd_features) = \
                struct.unpack_from(self._IOMMU_AMD_FORMAT, iommu_struct[offs:])
        elif self.type == JAILHOUSE_IOMMU.TIPVU:
            (self.tipvu_tlb_base,
             self.tipvu_tlb_size) = \
                struct.unpack_from(self._IOMMU_TIPVU_FORMAT,
                                   iommu_struct[offs:])
        elif not self.type in (JAILHOUSE_IOMMU.UNUSED,
                               JAILHOUSE_IOMMU.INTEL,
                               JAILHOUSE_IOMMU.SMMUV3,
                               JAILHOUSE_IOMMU.ARM_MMU500):
            raise RuntimeError('Unknown IOMMU type: %d' % self.type)


class SystemConfig:
    _HEADER_FORMAT = '=5sBH4x'
    # ...followed by MemRegion as hypervisor memory
    _CONSOLE_FORMAT = '32x'
    _PCI_FORMAT = '=QBBH'
    _NUM_IOMMUS = 8
    _ARCH_ARM_FORMAT = '=BB2xQQQQQ'
    _ARCH_X86_FORMAT = '=HBxIII28x'

    def __init__(self, data):
        self.data = data

        try:
            (signature, self.arch, revision) = \
                struct.unpack_from(self._HEADER_FORMAT, self.data)

            if signature != b'JHSYS':
                raise RuntimeError('Not a root cell configuration')
            if revision != _CONFIG_REVISION:
                raise RuntimeError('Configuration file revision mismatch')
            self.arch = convert_arch(self.arch)

            offs = struct.calcsize(self._HEADER_FORMAT)
            self.hypervisor_memory = MemRegion(self.data[offs:])

            offs += struct.calcsize(MemRegion._REGION_FORMAT)
            offs += struct.calcsize(self._CONSOLE_FORMAT)
            (self.pci_mmconfig_base,
             self.pci_mmconfig_end_bus,
             self.pci_is_virtual,
             self.pci_domain) = \
                 struct.unpack_from(self._PCI_FORMAT, self.data[offs:])

            offs += struct.calcsize(self._PCI_FORMAT)
            self.iommus = []
            for n in range(self._NUM_IOMMUS):
                iommu = IOMMU(self.data[offs:])
                if iommu.type != JAILHOUSE_IOMMU.UNUSED:
                    self.iommus.append(iommu)
                offs += IOMMU.SIZE

            if self.arch in ('arm', 'arm64'):
                (self.arm_maintenance_irq,
                 self.arm_gic_version,
                 self.arm_gicd_base,
                 self.arm_gicc_base,
                 self.arm_gich_base,
                 self.arm_gicv_base,
                 self.arm_gicr_base) = \
                     struct.unpack_from(self._ARCH_ARM_FORMAT, self.data[offs:])
            elif self.arch == 'x86':
                (self.x86_pm_timer_address,
                 self.x86_apic_mode,
                 self.x86_vtd_interrupt_limit,
                 self.x86_tsc_khz,
                 self.x86_apic_khz) = \
                     struct.unpack_from(self._ARCH_X86_FORMAT, self.data[offs:])

            offs += struct.calcsize(self._ARCH_ARM_FORMAT)
        except struct.error:
            raise RuntimeError('Not a root cell configuration')

        self.root_cell = CellConfig(self.data[offs:], root_cell=True)
