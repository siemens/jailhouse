#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2014-2017
# Copyright (c) Valentine Sinitsyn, 2014-2015
#
# Authors:
#  Henning Schild <henning.schild@siemens.com>
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Valentine Sinitsyn <valentine.sinitsyn@gmail.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# This script should help to create a basic jailhouse configuration file.
# It needs to be executed on the target machine, where it will gather
# information about the system. For more advanced scenarios you will have
# to change the generated C-code.


import re
import struct
import os
import fnmatch

from .pci_defs import PCI_CAP_ID, PCI_EXT_CAP_ID

root_dir = "/"
bdf_regex = re.compile(r'\w{4}:\w{2}:\w{2}\.\w')


def set_root_dir(dir):
    global root_dir
    root_dir = dir


inputs = {
    'files': set(),
    'files_opt': set(),
    'files_intel': set(),
    'files_amd': set()
}

# required files
inputs['files'].add('/proc/iomem')
inputs['files'].add('/proc/cpuinfo')
inputs['files'].add('/proc/cmdline')
inputs['files'].add('/proc/ioports')
inputs['files'].add('/sys/bus/pci/devices/*/config')
inputs['files'].add('/sys/bus/pci/devices/*/resource')
inputs['files'].add('/sys/devices/system/cpu/cpu*/uevent')
inputs['files'].add('/sys/firmware/acpi/tables/APIC')
inputs['files'].add('/sys/firmware/acpi/tables/MCFG')
# optional files
inputs['files_opt'].add('/sys/class/dmi/id/product_name')
inputs['files_opt'].add('/sys/class/dmi/id/sys_vendor')
inputs['files_opt'].add('/sys/class/tty/*/iomem_base')
inputs['files_opt'].add('/sys/class/tty/*/iomem_reg_shift')
inputs['files_opt'].add('/sys/class/tty/*/io_type')
inputs['files_opt'].add('/sys/class/tty/*/port')
inputs['files_opt'].add('/sys/devices/jailhouse/enabled')
# platform specific files
inputs['files_intel'].add('/sys/firmware/acpi/tables/DMAR')
inputs['files_amd'].add('/sys/firmware/acpi/tables/IVRS')


def check_input_listed(name, optional=False):
    set = inputs['files_opt']
    if optional is False:
        set = inputs['files']
        cpuvendor = get_cpu_vendor()
        if cpuvendor == 'GenuineIntel':
            set = set.union(inputs['files_intel'])
        elif cpuvendor == 'AuthenticAMD':
            set = set.union(inputs['files_amd'])

    for file in set:
        if fnmatch.fnmatch(name, file):
            return True
    raise RuntimeError('"' + name + '" is not a listed input file')


def input_open(name, mode='r', optional=False):
    check_input_listed(name, optional)
    try:
        f = open(root_dir + name, mode)
    except Exception as e:
        if optional:
            return open("/dev/null", mode)
        raise e
    return f


def input_listdir(dir, wildcards):
    for w in wildcards:
        check_input_listed(os.path.join(dir, w))
    dirs = os.listdir(root_dir + dir)
    dirs.sort()
    return dirs


def regions_split_by_kernel(tree):
    kernel = [x for x in tree.children
              if x.region.typestr.startswith('Kernel')]
    if not kernel:
        return [tree.region]

    r = tree.region
    s = r.typestr

    kernel_start = kernel[0].region.start
    kernel_stop = kernel[len(kernel) - 1].region.stop

    # align this for 16M, but only if we have enough space
    kernel_stop = (kernel_stop & ~0xFFFFFF) + 0xFFFFFF
    if kernel_stop > r.stop:
        kernel_stop = r.stop

    result = []

    # before Kernel if any
    if r.start < kernel_start:
        result.append(MemRegion(r.start, kernel_start - 1, s))

    result.append(MemRegion(kernel_start, kernel_stop, "Kernel"))

    # after Kernel if any
    if r.stop > kernel_stop:
        result.append(MemRegion(kernel_stop + 1, r.stop, s))

    return result


def parse_iomem_tree(tree):
    regions = []
    dmar_regions = []

    for tree in tree.children:
        r = tree.region
        s = r.typestr

        # System RAM on the first level will be added completely
        # if it doesn't contain the kernel itself. If it does,
        # we split it.
        if tree.level == 1 and s == 'System RAM':
            regions.extend(regions_split_by_kernel(tree))
            continue

        # blacklisted on all levels, covers both APIC and IOAPIC
        if s.find('PCI MMCONFIG') >= 0 or s.find('APIC') >= 0:
            continue

        # if the tree continues recurse further down ...
        if tree.children:
            (temp_regions, temp_dmar_regions) = parse_iomem_tree(tree)
            regions.extend(temp_regions)
            dmar_regions.extend(temp_dmar_regions)
            continue
        else:
            # blacklisted if it has no children
            if s.lower() == 'reserved':
                continue

        # add all remaining leaves
        regions.append(r)

    return regions, dmar_regions


def parse_iomem(pcidevices):
    tree = IORegionTree.parse_io_file('/proc/iomem', MemRegion)
    (regions, dmar_regions) = parse_iomem_tree(tree)

    rom_region = MemRegion(0xc0000, 0xdffff, 'ROMs')
    add_rom_region = False

    ret = []
    for r in regions:
        # Filter PCI buses in order to avoid mapping empty ones that might
        # require interception when becoming non-empty.
        # Exception: VGA region
        if r.typestr.startswith('PCI Bus') and r.start != 0xa0000:
            continue

        append_r = True
        # filter the list for MSI-X pages
        for d in pcidevices:
            if r.start <= d.msix_address <= r.stop:
                if d.msix_address > r.start:
                    head_r = MemRegion(r.start, d.msix_address - 1,
                                       r.typestr, r.comments)
                    ret.append(head_r)
                if d.msix_address + d.msix_region_size < r.stop:
                    tail_r = MemRegion(d.msix_address + d.msix_region_size,
                                       r.stop, r.typestr, r.comments)
                    ret.append(tail_r)
                append_r = False
                break
        # filter out the ROMs
        if r.start >= rom_region.start and r.stop <= rom_region.stop:
            add_rom_region = True
            append_r = False
        # filter out and save DMAR regions
        if r.typestr.find('dmar') >= 0:
            dmar_regions.append(r)
            append_r = False
        # filter out AMD IOMMU regions
        if r.typestr.find('amd_iommu') >= 0:
            append_r = False
        # merge adjacent RAM regions
        if append_r and len(ret) > 0:
            prev = ret[-1]
            if prev.is_ram() and r.is_ram() and prev.stop + 1 == r.start:
                prev.stop = r.stop
                append_r = False
        if append_r:
            ret.append(r)

    # add a region that covers all potential ROMs
    if add_rom_region:
        ret.append(rom_region)

    # newer Linux kernels will report the first page as reserved
    # it is needed for CPU init so include it anyways
    if ret[0].typestr == 'System RAM' and ret[0].start == 0x1000:
        ret[0].start = 0

    return ret, dmar_regions


def ioports_search_pci_devices(tree):
    ret = []

    if tree.region and bdf_regex.match(tree.region.typestr):
        ret.append(tree.region)
    else:
        for subtree in tree:
            ret += ioports_search_pci_devices(subtree)

    return ret


def parse_ioports():
    tree = IORegionTree.parse_io_file('/proc/ioports', PortRegion)

    pm_timer_base = tree.find_regions_by_name('ACPI PM_TMR')
    if len(pm_timer_base) != 1:
        raise RuntimeError('Found %u entries for ACPI PM_TMR (expected 1)' %
                           len(pm_timer_base))
    pm_timer_base = pm_timer_base[0].start

    leaves = tree.get_leaves()

    # Never expose PCI config space ports to the user
    leaves = list(filter(lambda p: p.start != 0xcf8, leaves))

    # Drop everything above 0xd00
    leaves = list(filter(lambda p: p.start < 0xd00, leaves))

    whitelist = [
        0x40,   # PIT
        0x60,   # keyboard
        0x61,   # HACK: NMI status/control
        0x64,   # I8042
        0x70,   # RTC
        0x2f8,  # serial
        0x3f8,  # serial
    ]

    pci_devices = ioports_search_pci_devices(tree)

    # Drop devices below 0xd00 as leaves already contains them. Access should
    # not be permitted by default.
    pci_devices = list(filter(lambda p: p.start >= 0xd00, pci_devices))
    for pci_device in pci_devices:
        pci_device.permit = True

    for r in leaves:
        typestr = r.typestr.lower()
        if r.start in whitelist or \
           True in [vga in typestr for vga in ['vesa', 'vga']]:
            r.permit = True

    leaves += pci_devices
    leaves.sort(key=lambda r: r.start)

    return leaves, pm_timer_base


def parse_pcidevices():
    devices = []
    basedir = '/sys/bus/pci/devices'
    list = input_listdir(basedir, ['*/config'])
    for dir in list:
        d = PCIDevice.parse_pcidevice_sysfsdir(basedir, dir)
        if d is not None:
            devices.append(d)
    return devices


def parse_madt():
    f = input_open('/sys/firmware/acpi/tables/APIC', 'rb')
    signature = f.read(4)
    if signature != b'APIC':
        raise RuntimeError('MADT: incorrect input file format %s' % signature)
    (length,) = struct.unpack('<I', f.read(4))
    f.seek(44)
    length -= 44
    ioapics = []

    while length > 0:
        offset = 0
        (struct_type, struct_len) = struct.unpack('<BB', f.read(2))
        offset += 2
        length -= struct_len

        if struct_type == 1:
            (id, address, gsi_base) = struct.unpack('<BxII', f.read(10))
            offset += 10
            ioapics.append(IOAPIC(id, address, gsi_base))

        f.seek(struct_len - offset, os.SEEK_CUR)

    f.close()
    return ioapics


def parse_dmar_devscope(f):
    (scope_type, scope_len, id, bus, dev, fn) = \
        struct.unpack('<BBxxBBBB', f.read(8))
    if scope_len != 8:
        raise RuntimeError('Unsupported DMAR Device Scope Structure')
    return (scope_type, scope_len, id, bus, dev, fn)


# parsing of DMAR ACPI Table
# see Intel VT-d Spec chapter 8
def parse_dmar(pcidevices, ioapics, dmar_regions):
    f = input_open('/sys/firmware/acpi/tables/DMAR', 'rb')
    signature = f.read(4)
    if signature != b'DMAR':
        raise RuntimeError('DMAR: incorrect input file format %s' % signature)
    (length,) = struct.unpack('<I', f.read(4))
    f.seek(48)
    length -= 48
    units = []
    regions = []

    while length > 0:
        offset = 0
        (struct_type, struct_len) = struct.unpack('<HH', f.read(4))
        offset += 4
        length -= struct_len

        # DMA Remapping Hardware Unit Definition
        if struct_type == 0:
            (flags, segment, base) = struct.unpack('<BxHQ', f.read(12))
            if segment != 0:
                raise RuntimeError('We do not support multiple PCI segments')
            if len(units) >= 8:
                raise RuntimeError('Too many DMAR units. '
                                   'Raise JAILHOUSE_MAX_IOMMU_UNITS.')
            size = 0
            for r in dmar_regions:
                if base == r.start:
                    size = r.size()
            if size == 0:
                raise RuntimeError('DMAR region size cannot be identified.\n'
                                   'Target Linux must run with Intel IOMMU '
                                   'enabled.')
            if size > 0x3000:
                raise RuntimeError('Unexpectedly large DMAR region.')
            units.append(IOMMUConfig({
                'type': 'JAILHOUSE_IOMMU_INTEL',
                'base_addr': base,
                'mmio_size': size
            }))
            if flags & 1:
                for d in pcidevices:
                    if d.iommu is None:
                        d.iommu = len(units) - 1
            offset += 16 - offset
            while offset < struct_len:
                (scope_type, scope_len, id, bus, dev, fn) =\
                    parse_dmar_devscope(f)
                # PCI Endpoint Device
                if scope_type == 1:
                    assert not (flags & 1)
                    for d in pcidevices:
                        if d.bus == bus and d.dev == dev and d.fn == fn:
                            d.iommu = len(units) - 1
                            break
                # PCI Sub-hierarchy
                elif scope_type == 2:
                    assert not (flags & 1)
                    for d in pcidevices:
                        if d.bus == bus and d.dev == dev and d.fn == fn:
                            d.iommu = len(units) - 1
                            (secondbus, subordinate) = \
                                PCIPCIBridge.get_2nd_busses(d)
                            for d2 in pcidevices:
                                if (
                                        d2.bus >= secondbus and
                                        d2.bus <= subordinate
                                ):
                                    d2.iommu = len(units) - 1
                            break
                # IOAPIC
                elif scope_type == 3:
                    ioapic = next(chip for chip in ioapics if chip.id == id)
                    bdf = (bus << 8) | (dev << 3) | fn
                    for chip in ioapics:
                        if chip.bdf == bdf:
                            raise RuntimeError('IOAPICs with identical BDF')
                    ioapic.bdf = bdf
                    ioapic.iommu = len(units) - 1
                offset += scope_len

        # Reserved Memory Region Reporting Structure
        if struct_type == 1:
            f.seek(8 - offset, os.SEEK_CUR)
            offset += 8 - offset
            (base, limit) = struct.unpack('<QQ', f.read(16))
            offset += 16

            comments = []
            while offset < struct_len:
                (scope_type, scope_len, id, bus, dev, fn) =\
                    parse_dmar_devscope(f)
                if scope_type == 1:
                    comments.append('PCI device: %02x:%02x.%x' %
                                    (bus, dev, fn))
                else:
                    comments.append('DMAR parser could not decode device path')
                offset += scope_len

            reg = MemRegion(base, limit, 'ACPI DMAR RMRR', comments)
            regions.append(reg)

        f.seek(struct_len - offset, os.SEEK_CUR)

    f.close()

    for d in pcidevices:
        if d.iommu is None:
            raise RuntimeError(
                'PCI device %02x:%02x.%x outside the scope of an '
                'IOMMU' % (d.bus, d.dev, d.fn))

    return units, regions


def parse_ivrs(pcidevices, ioapics):
    def format_bdf(bdf):
        bus, dev, fun = (bdf >> 8) & 0xff, (bdf >> 3) & 0x1f, bdf & 0x7
        return '%02x:%02x.%x' % (bus, dev, fun)

    f = input_open('/sys/firmware/acpi/tables/IVRS', 'rb')
    signature = f.read(4)
    if signature != b'IVRS':
        raise RuntimeError('IVRS: incorrect input file format %s' % signature)

    (length, revision) = struct.unpack('<IB', f.read(5))
    if revision > 2:
        raise RuntimeError('IVRS: unsupported Revision %02x' % revision)

    f.seek(48, os.SEEK_SET)
    length -= 48

    units = []
    regions = []
    # BDF of devices that are permitted outside IOMMU: root complex
    iommu_skiplist = set([0x0])
    ivhd_blocks = 0
    while length > 0:
        (block_type, block_length) = struct.unpack('<BxH', f.read(4))
        if block_type in [0x10, 0x11]:
            ivhd_blocks += 1
            if ivhd_blocks > 1:
                raise RuntimeError('Jailhouse doesn\'t support more than one '
                                   'AMD IOMMU per PCI function.')
            # IVHD block
            ivhd_fields = struct.unpack('<HHQHxxL', f.read(20))
            (iommu_bdf, base_cap_ofs,
             base_addr, pci_seg, iommu_feat) = ivhd_fields

            length -= block_length
            block_length -= 24

            if pci_seg != 0:
                raise RuntimeError('We do not support multiple PCI segments')

            if len(units) > 8:
                raise RuntimeError('Too many IOMMU units. '
                                   'Raise JAILHOUSE_MAX_IOMMU_UNITS.')

            msi_cap_ofs = None

            for i, d in enumerate(pcidevices):
                if d.bdf() == iommu_bdf:
                    # Extract MSI capability offset
                    for c in d.caps:
                        if c.id == PCI_CAP_ID.MSI:
                            msi_cap_ofs = c.start
                    # We must not map IOMMU to the cells
                    del pcidevices[i]

            if msi_cap_ofs is None:
                raise RuntimeError('AMD IOMMU lacks MSI support, and '
                                   'Jailhouse doesn\'t support MSI-X yet.')

            if (iommu_feat & (0xF << 13)) and (iommu_feat & (0x3F << 17)):
                # Performance Counters are supported, allocate 512K
                mmio_size = 524288
            else:
                # Allocate 16K
                mmio_size = 16384

            units.append(IOMMUConfig({
                'type': 'JAILHOUSE_IOMMU_AMD',
                'base_addr': base_addr,
                'mmio_size': mmio_size,
                'amd_bdf': iommu_bdf,
                'amd_base_cap': base_cap_ofs,
                'amd_msi_cap': msi_cap_ofs,
                # IVHD block type 0x11 has exact EFR copy but type 0x10 may
                # overwrite what hardware reports. Set reserved bit 0 in that
                # case to indicate that the value is in use.
                'amd_features': (iommu_feat | 0x1) if block_type == 0x10 else 0
            }))

            bdf_start_range = None
            while block_length > 0:
                (entry_type, device_id) = struct.unpack('<BHx', f.read(4))
                block_length -= 4

                if entry_type == 0x01:
                    # All
                    for d in pcidevices:
                        d.iommu = len(units) - 1
                elif entry_type == 0x02:
                    # Select
                    for d in pcidevices:
                        if d.bdf() == device_id:
                            d.iommu = len(units) - 1
                elif entry_type == 0x03:
                    # Start of range
                    bdf_start_range = device_id
                elif entry_type == 0x04:
                    # End of range
                    if bdf_start_range is None:
                        continue
                    for d in pcidevices:
                        if d.bdf() >= bdf_start_range and d.bdf() <= device_id:
                            d.iommu = len(units) - 1
                    bdf_start_range = None
                elif entry_type == 0x42:
                    # Alias select
                    (device_id_b,) = struct.unpack('<xHx', f.read(4))
                    block_length -= 4
                    for d in pcidevices:
                        if d.bdf() == device_id_b:
                            d.iommu = len(units) - 1
                elif entry_type == 0x43:
                    # Alias start of range
                    (device_id_b,) = struct.unpack('<xHx', f.read(4))
                    block_length -= 4
                    bdf_start_range = device_id_b
                elif entry_type == 0x48:
                    # Special device
                    (handle, device_id_b, variety) = struct.unpack(
                        '<BHB', f.read(4))
                    block_length -= 4
                    if variety == 0x01:  # IOAPIC
                        for chip in ioapics:
                            if chip.id == handle:
                                chip.bdf = device_id_b
                                chip.iommu = len(units) - 1
                else:
                    # Reserved or ignored entries
                    if entry_type >= 0x40:
                        f.seek(4, os.SEEK_CUR)
                        block_length -= 4

        elif type in [0x20, 0x21, 0x22]:
            # IVMD block
            ivmd_fields = struct.unpack('<BBHHHxxxxxxxxQQ', f.read(32))
            (block_type, block_flags, block_length,
             device_id, aux_data, mem_addr, mem_len) = ivmd_fields
            length -= block_length

            if int(block_flags):
                bdf_str = format_bdf(device_id)
                print(
                    'WARNING: Jailhouse doesn\'t support configurable '
                    '(eg. read-only) device memory. Device %s may not '
                    'work properly, especially in non-root cell.' % bdf_str)

            if block_type == 0x20:
                # All devices
                comment = None
            elif block_type == 0x21:
                # Selected device
                comment = 'PCI Device: %s' % format_bdf(device_id)
            elif block_type == 0x22:
                # Device range
                comment = 'PCI Device: %s - %s' % (
                    format_bdf(device_id), format_bdf(aux_data))

            if comment:
                print('WARNING: Jailhouse doesn\'t support per-device memory '
                      'regions. The memory at 0x%x will be mapped accessible '
                      'to all devices.' % mem_addr)

            regions.append(
                MemRegion(mem_addr, mem_addr + mem_len - 1, 'ACPI IVRS',
                          comment))

        elif type == 0x40:
            raise RuntimeError(
                'You board uses IVRS Rev. 2 feature Jailhouse doesn\'t '
                'support yet. Please report this to '
                'jailhouse-dev@googlegroups.com.')
        else:
            print(
                'WARNING: Skipping unknown IVRS '
                'block type 0x%02x' % block_type)

        for d in pcidevices:
            if d.bdf() not in iommu_skiplist and d.iommu is None:
                raise RuntimeError(
                    'PCI device %02x:%02x.%x outside the scope of an '
                    'IOMMU' % (d.bus, d.dev, d.fn))

        f.close()
        return units, regions


def get_cpu_vendor():
    with open(root_dir + '/proc/cpuinfo') as f:
        for line in f:
            if not line.strip():
                continue
            key, value = line.split(':')
            if key.strip() == 'vendor_id':
                return value.strip()
    raise RuntimeError('Broken %s/proc/cpuinfo' % root_dir)


class PCIBARs:
    IORESOURCE_IO = 0x00000100
    IORESOURCE_MEM = 0x00000200
    IORESOURCE_MEM_64 = 0x00100000

    def __init__(self, dir):
        self.mask = []
        f = input_open(os.path.join(dir, 'resource'), 'r')
        n = 0
        while (n < 6):
            (start, end, flags) = f.readline().split()
            n += 1
            flags = int(flags, 16)
            if flags & PCIBARs.IORESOURCE_IO:
                mask = ~(int(end, 16) - int(start, 16))
            elif flags & PCIBARs.IORESOURCE_MEM:
                if flags & PCIBARs.IORESOURCE_MEM_64:
                    mask = int(end, 16) - int(start, 16)
                    (start, end, flags) = f.readline().split()
                    mask |= (int(end, 16) - int(start, 16)) << 32
                    mask = ~(mask)
                    self.mask.append(mask & 0xffffffff)
                    mask >>= 32
                    n += 1
                else:
                    mask = ~(int(end, 16) - int(start, 16))
            else:
                mask = 0
            self.mask.append(mask & 0xffffffff)
        f.close()


class PCICapability:
    def __init__(self, id, extended, start, len, flags, content, msix_address):
        self.id = id
        self.extended = extended
        self.start = start
        self.len = len
        self.flags = flags
        self.content = content
        self.msix_address = msix_address
        self.comments = []

    def __eq__(self, other):
        return self.id == other.id and self.start == other.start and \
            self.len == other.len and self.flags == other.flags

    def gen_id_str(self):
        return str(self.id) + \
            (' | JAILHOUSE_PCI_EXT_CAP' if self.extended else '')

    RD = '0'
    RW = 'JAILHOUSE_PCICAPS_WRITE'

    JAILHOUSE_PCI_EXT_CAP = 0x8000

    @staticmethod
    def parse_pcicaps(dir):
        caps = []
        has_extended_caps = False
        f = input_open(os.path.join(dir, 'config'), 'rb')
        f.seek(0x06)
        (status,) = struct.unpack('<H', f.read(2))
        # capability list supported?
        if (status & (1 << 4)) == 0:
            f.close()
            return caps
        # walk capability list
        f.seek(0x34)
        (next,) = struct.unpack('B', f.read(1))
        while next != 0:
            cap = next
            msix_address = 0
            f.seek(cap)
            (id, next) = struct.unpack('<BB', f.read(2))
            id = PCI_CAP_ID(id)
            if id == PCI_CAP_ID.PM:
                # this cap can be handed out completely
                len = 8
                flags = PCICapability.RW
            elif id == PCI_CAP_ID.MSI:
                # access will be moderated by hypervisor
                len = 10
                (msgctl,) = struct.unpack('<H', f.read(2))
                if (msgctl & (1 << 7)) != 0:  # 64-bit support
                    len += 4
                if (msgctl & (1 << 8)) != 0:  # per-vector masking support
                    len += 10
                flags = PCICapability.RW
            elif id == PCI_CAP_ID.EXP:
                len = 20
                (cap_reg,) = struct.unpack('<H', f.read(2))
                if (cap_reg & 0xf) >= 2:  # v2 capability
                    len = 60
                # access side effects still need to be analyzed
                flags = PCICapability.RD
                has_extended_caps = True
            elif id == PCI_CAP_ID.MSIX:
                # access will be moderated by hypervisor
                len = 12
                (table,) = struct.unpack('<xxI', f.read(6))
                f.seek(0x10 + (table & 7) * 4)
                (bar,) = struct.unpack('<I', f.read(4))
                if (bar & 0x3) != 0:
                    raise RuntimeError('Invalid MSI-X BAR found')
                if (bar & 0x4) != 0:
                    bar |= struct.unpack('<I', f.read(4))[0] << 32
                msix_address = \
                    (bar & 0xfffffffffffffff0) + (table & 0xfffffff8)
                flags = PCICapability.RW
            else:
                # unknown/unhandled cap, mark its existence
                len = 2
                flags = PCICapability.RD
            f.seek(cap + 2)
            content = f.read(len - 2)
            caps.append(PCICapability(id, False, cap, len, flags, content,
                                      msix_address))

        if has_extended_caps:
            # walk extended capability list
            next = 0x100
            while next != 0:
                cap = next
                f.seek(cap)
                (id, version_next) = struct.unpack('<HH', f.read(4))
                if id == 0xffff:
                    break
                elif (id & PCICapability.JAILHOUSE_PCI_EXT_CAP) != 0:
                    print('WARNING: Ignoring unsupported PCI Express '
                          'Extended Capability ID %x' % id)
                    continue

                id = PCI_EXT_CAP_ID(id)
                next = version_next >> 4
                # access side effects still need to be analyzed
                flags = PCICapability.RD

                if id == PCI_EXT_CAP_ID.VNDR:
                    (vsec_len,) = struct.unpack('<I', f.read(4))
                    len = 4 + (vsec_len >> 20)
                elif id == PCI_EXT_CAP_ID.ACS:
                    len = 8

                    (acs_cap, acs_ctrl) = struct.unpack('<HH', f.read(4))
                    if acs_cap & (1 << 5) and acs_ctrl & (1 << 5):
                        vector_bits = acs_cap >> 8
                        if vector_bits == 0:
                            vector_bits = 256

                        vector_bytes = int((vector_bits + 31) / (8 * 4))
                        len += vector_bytes
                elif id in [PCI_EXT_CAP_ID.VC, PCI_EXT_CAP_ID.VC9]:
                    # parsing is too complex, but we have at least 4 DWORDS
                    len = 4 * 4
                elif id == PCI_EXT_CAP_ID.MFVC:
                    len = 4
                elif id in [PCI_EXT_CAP_ID.LTR, PCI_EXT_CAP_ID.ARI,
                            PCI_EXT_CAP_ID.PASID]:
                    len = 8
                elif id in [PCI_EXT_CAP_ID.DSN, PCI_EXT_CAP_ID.PTM]:
                    len = 12
                elif id in [PCI_EXT_CAP_ID.PWR, PCI_EXT_CAP_ID.SECPCI]:
                    len = 16
                elif id == PCI_EXT_CAP_ID.MCAST:
                    len = 48
                elif id in [PCI_EXT_CAP_ID.SRIOV, PCI_EXT_CAP_ID.ERR]:
                    len = 64
                else:
                    # unknown/unhandled cap, mark its existence
                    len = 4
                f.seek(cap + 4)
                content = f.read(len - 4)
                caps.append(PCICapability(id, True, cap, len, flags, content,
                                          0))

        f.close()
        return caps


class PCIDevice:
    def __init__(self, type, domain, bus, dev, fn, bars, caps, path):
        self.type = type
        self.iommu = None
        self.domain = domain
        self.bus = bus
        self.dev = dev
        self.fn = fn
        self.bars = bars
        self.caps = caps
        self.path = path
        self.caps_start = 0
        self.num_caps = len(caps)
        self.num_msi_vectors = 0
        self.msi_64bits = 0
        self.msi_maskable = 0
        self.num_msix_vectors = 0
        self.msix_region_size = 0
        self.msix_address = 0
        for c in caps:
            if c.id in (PCI_CAP_ID.MSI, PCI_CAP_ID.MSIX):
                msg_ctrl = struct.unpack('<H', c.content[:2])[0]
                if c.id == PCI_CAP_ID.MSI:
                    self.num_msi_vectors = 1 << ((msg_ctrl >> 1) & 0x7)
                    self.msi_64bits = (msg_ctrl >> 7) & 1
                    self.msi_maskable = (msg_ctrl >> 8) & 1
                else:  # MSI-X
                    if c.msix_address != 0:
                        vectors = (msg_ctrl & 0x7ff) + 1
                        self.num_msix_vectors = vectors
                        self.msix_region_size = (vectors * 16 + 0xfff) & 0xf000
                        self.msix_address = c.msix_address
                    else:
                        print('WARNING: Ignoring invalid MSI-X configuration'
                              ' of device %02x:%02x.%x' % (bus, dev, fn))

    def __str__(self):
        return 'PCIDevice: %02x:%02x.%x' % (self.bus, self.dev, self.fn)

    def bdf(self):
        return self.bus << 8 | self.dev << 3 | self.fn

    @staticmethod
    def parse_pcidevice_sysfsdir(basedir, dir):
        dpath = os.path.join(basedir, dir)
        f = input_open(os.path.join(dpath, 'config'), 'rb')
        (vendor_device,) = struct.unpack('<I', f.read(4))
        if vendor_device == 0xffffffff:
            print('WARNING: Ignoring apparently disabled PCI device %s' % dir)
            return None
        f.seek(0x0A)
        (classcode,) = struct.unpack('<H', f.read(2))
        f.close()
        if classcode == 0x0604:
            type = 'JAILHOUSE_PCI_TYPE_BRIDGE'
        else:
            type = 'JAILHOUSE_PCI_TYPE_DEVICE'
        a = dir.split(':')
        domain = int(a[0], 16)
        bus = int(a[1], 16)
        df = a[2].split('.')
        bars = PCIBARs(dpath)
        caps = PCICapability.parse_pcicaps(dpath)
        return PCIDevice(type, domain, bus, int(df[0], 16), int(df[1], 16),
                         bars, caps, dpath)


class PCIPCIBridge(PCIDevice):
    @staticmethod
    def get_2nd_busses(dev):
        assert dev.type == 'JAILHOUSE_PCI_TYPE_BRIDGE'
        f = input_open(os.path.join(dev.path, 'config'), 'rb')
        f.seek(0x19)
        (secondbus, subordinate) = struct.unpack('<BB', f.read(2))
        f.close()
        return (secondbus, subordinate)


class IORegion(object):
    def __init__(self, start, stop, typestr, comments):
        self.start = start
        self.stop = stop
        self.typestr = typestr
        self.comments = comments or []

    def __str__(self):
        return self.typestr

    def size(self):
        return int(self.stop - self.start + 1)

    def start_str(self):
        # This method is used in root-cell-config.c.tmpl
        return hex(self.start)

    def size_str(self):
        # Comments from start_str() apply here as well.
        return hex(self.size())


class MemRegion(IORegion):
    def __init__(self, start, stop, typestr, comments=None):
        super(MemRegion, self).__init__(start, stop, typestr, comments)

    def __str__(self):
        return 'MemRegion: %08x-%08x : %s' % \
            (self.start, self.stop, super(MemRegion, self).__str__())

    def size(self):
        # round up to full PAGE_SIZE
        return int((super(MemRegion, self).size() + 0xfff) / 0x1000) * 0x1000

    def is_ram(self):
        return (self.typestr == 'System RAM' or
                self.typestr == 'Kernel' or
                self.typestr == 'RAM buffer' or
                self.typestr == 'ACPI DMAR RMRR' or
                self.typestr == 'ACPI IVRS')

    def flagstr(self, p=''):
        if self.is_ram():
            return 'JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |\n' + \
                p + '\t\tJAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA'
        else:
            return 'JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE'


class PortRegion(IORegion):
    def __init__(self, start, stop, typestr, permit=False, comments=None):
        super(PortRegion, self).__init__(start, stop, typestr, comments)
        self.permit = permit

    def __str__(self):
        return 'Port I/O: %04x-%04x : %s' % \
            (self.start, self.stop, super(PortRegion, self).__str__())


class IOAPIC:
    def __init__(self, id, address, gsi_base, iommu=0, bdf=0):
        self.id = id
        self.address = address
        self.gsi_base = gsi_base
        self.iommu = iommu
        self.bdf = bdf

    def __str__(self):
        return 'IOAPIC %d, GSI base %d' % (self.id, self.gsi_base)

    def irqchip_id(self):
        # encode the IOMMU number into the irqchip ID
        return (self.iommu << 16) | self.bdf


class IORegionTree:
    def __init__(self, region, level):
        self.region = region
        self.level = level
        self.parent = None
        self.children = []

    def __iter__(self):
        for child in self.children:
            yield child

    def get_leaves(self):
        leaves = []

        if self.children:
            for child in self.children:
                leaves.extend(child.get_leaves())
        elif self.region is not None:
            leaves.append(self.region)

        return leaves

    # find specific regions in tree
    def find_regions_by_name(self, name):
        regions = []

        for tree in self.children:
            r = tree.region
            s = r.typestr

            if s.find(name) >= 0:
                regions.append(r)

            # if the tree continues recurse further down ...
            if tree.children:
                regions.extend(tree.find_regions_by_name(name))

        return regions

    @staticmethod
    def parse_io_line(line, TargetClass):
        (region, type) = line.split(' : ', 1)
        level = int(region.count(' ') / 2) + 1
        type = type.strip()
        region = [r.strip() for r in region.split('-', 1)]

        return level, TargetClass(int(region[0], 16), int(region[1], 16), type)

    @staticmethod
    def parse_io_file(filename, TargetClass):
        root = IORegionTree(None, 0)
        f = input_open(filename)
        lastlevel = 0
        lastnode = root
        for line in f:
            (level, r) = IORegionTree.parse_io_line(line, TargetClass)
            t = IORegionTree(r, level)
            if t.level > lastlevel:
                t.parent = lastnode
            if t.level == lastlevel:
                t.parent = lastnode.parent
            if t.level < lastlevel:
                p = lastnode.parent
                while t.level < p.level:
                    p = p.parent
                t.parent = p.parent

            t.parent.children.append(t)
            lastnode = t
            lastlevel = t.level
        f.close()

        return root


class IOMMUConfig:
    def __init__(self, props):
        self.type = props['type']
        self.base_addr = props['base_addr']
        self.mmio_size = props['mmio_size']
        if 'amd_bdf' in props:
            self.amd_bdf = props['amd_bdf']
            self.amd_base_cap = props['amd_base_cap']
            self.amd_msi_cap = props['amd_msi_cap']
            self.amd_features = props['amd_features']

    @property
    def is_amd_iommu(self):
        return hasattr(self, 'amd_bdf')
