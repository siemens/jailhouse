Inter-Cell communication of the Jailhouse Hypervisor
====================================================

The hypervisor isolates cells but sometimes there is a need to exchange data
between cells. For that purpose Jailhouse provides shared memory and signaling
between cells via virtual ivshmem PCI devices. This device type is specified
in [1].


Adding Inter-cell communication
-------------------------------

In order to set up a communication channel between two or more cells you first
have to add memory regions to both cells. Each cell needs the follow regions:

 - read-only region to hold state table, generally one page large
 - one region that is read/writable for all peers
 - one output region per peer that is only read-writeable for one of them

With 2 peers connected, this means a consecutive series of 4 regions needs to
be created. When connecting 3 peers, 5 regions are needed, and so on. All
regions must also be consecutive in guest-physical memory because only the
address for the first region is communicated to the guest.

The second, common read-write region is optional and can be zero-sized. Also
the output regions are optional. All output regions must have the same size.
Write permission to the first output region must only be granted to the cell
that has the ivshmem device ID 0, write permission to the second region must be
granted to ID 1, and so forth.

Non-root cells sharing memory with the root cell need the memory flag
`JAILHOUSE_MEM_ROOTSHARED` on the region.

To define the memory regions of an ivshmem-net device, the macro
`JAILHOUSE_SHMEM_NET_REGIONS(base_address, id)` is provided. It uses 1 MB of
memory at the specified base address and assigns access according to the
specified ID. Shared memory based network devices only connect 2 peers, thus
4 memory regions will be added.

After creating the memory regions, also a PCI device needs to be added to each
connected cell. Set the device type to `JAILHOUSE_PCI_TYPE_IVSHMEM`. The `bdf`
field must specify an unused bus-device-function slot on a physical or virtual
PCI controller. All connected peers must use the same `bdf` value in order to
establish the link. They may use different `domain` values, though.

Set the bar_mask to either `JAILHOUSE_IVSHMEM_BAR_MASK_MSIX` or
`JAILHOUSE_IVSHMEM_BAR_MASK_INTX`, depending on whether MSI-X is available or
not. When MSI-X is used, num_msix_vectors must be set according to the needs of
the shared memory protocol used on the link. For ivshmem networking, grant 2
vectors.

Further fields needed:
 - `shmem_regions_start` - index of first shared memory region used by device
 - `shmem_dev_id` - ID of the peer (0..`shmem_peers`-1)
 - `shmem_peers` - maximum number of connected peers
 - `shmem_protocol` - shared memory protocol used over the link

Set `shmem_protocol` to JAILHOUSE_SHMEM_PROTO_VETH for ivshmem networking, use
`JAILHOUSE_SHMEM_PROTO_UNDEFINED` for custom protocols, or pick an ID from the
custom range defined in [1].

You may also need to set the `iommu` field to match the IOMMU unit that the
guest expects based on the `bdf` value. Try 1 if MSI-X interrupts do not make
it when using 0.

For an example have a look at the cell configuration files `qemu-x86.c`,
`ivshmem-demo.c`, and `linux-x86-demo.c` in `configs/x86`.


Demo code
---------

TODO


References
----------

[1] Documentation/ivshmem-v2-specification.md
