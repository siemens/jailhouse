Cache Coloring Support
======================

Introduction
------------

### Cache partitioning and coloring

#### Motivation

Cache hierarchies of modern multi-core CPUs typically have first levels
dedicated
to each core (hence using multiple cache units), while the last level cache
(LLC) is shared among all of them. Such configuration implies that memory
operations on one core, e.g., running one Jailhouse inmate, are able to generate
timing *interference* on another core, e.g., hosting another inmate. More
specifically, data cached by the latter core can be evicted by cache store
operations performed by the former. In practice, this means that the memory
latency experienced by one core depends on the other cores (in-)activity.

The obvious solution is to provide hardware mechanisms allowing either: a
fine-grained control with cache lock-down, as offered on the previous v7
generation of Arm architectures; or a coarse-grained control with LLC
partitioning among different cores, as featured on the "Cache Allocation
Technology" of the high-end segment of recent Intel architecture and supported
by the Jailhouse hypervisor.

#### Cache coloring

Cache coloring is a *software technique* that permits LLC partitioning,
therefore eliminating mutual core interference, and thus guaranteeing higher and
more predictable performances for memory accesses. A given memory space in
central memory is partioned into subsets called colors, so that addresses in
different colors are necessarily cached in different LLC lines. On Arm
architectures, colors are easily defined by the following circular striding.

```
          _ _ _______________ _ _____________________ _ _
               |     |     |     |     |     |     |
               | c_0 | c_1 |     | c_n | c_0 | c_1 |
          _ _ _|_____|_____|_ _ _|_____|_____|_____|_ _ _
                  :                       :
                  '......         ........'
                        . color 0 .
                . ........      ............... .
                         :      :
            . ...........:      :..................... .
```

Cache coloring suffices to define separate domains that are guaranteed to be
*free from interference* with respect to the mutual evictions, but it does not
protect from minor interference effects still present on LLC shared
subcomponents (almost negligible), nor from the major source of contention
present in central memory.

It is also worth remarking that cache coloring also partitions the central
memory availability accordingly to the color allocation--assigning, for
instance, half of the LLC size is possible if and only if half of the DRAM space
is assigned, too.


### Cache coloring in Jailhouse

The *cache coloring support in Jailhouse* allows partitioning the cache by
simply partitioning the colors available on the specific platform, whose number
may vary depending on the specific cache implementation. More detail about color
availability and selection is provided in [Usage](#usage).

#### Supported architectures

Cache coloring is available on Arm64 architectures. In particular, extensive
testing has been performed on v8 CPUs, namely on the A53 and A57 processors
equipping Xilinx ZCU102 and ZCU104.

#### Limitations

Since Jailhouse is currently lacking SMMU support, and since the colored memory
mapping must be provided to DMA devices to allow them a coherent memory view,
coloring for this kind of devices is not available.
This also explain why also coloring support for the Linux root cell is not
provided, although possible and tested with a simple hot remapping procedure.

### Further readings

Relevance, applicability, and evaluation results of the Jailhouse cache coloring
support are reported in several recent works. A non-technical perspective is
given in [1] together with an overview of the ambitious HERCULES research
project. A technical and scientific presentation is instead authored in [2],
where additional experimental techniques on cache and DRAM are introduced. A
specific real-time application is extensively discussed in [3].

An enjoyable, comprehensive and up-to-date survey on cache management technique
for
real-time systems is offered by [4].

1. P. Gai, C. Scordino, M. Bertogna, M. Solieri, T. Kloda, L. Miccio. 2019.
   "Handling Mixed Criticality on Modern Multi-core Systems: the HERCULES
   Project", Embedded World Exhibition and Conference 2019.

2. T. Kloda, M. Solieri, R. Mancuso, N. Capodieci, P. Valente, M. Bertogna.
   2019.
   "Deterministic Memory Hierarchy and Virtualization for Modern Multi-Core
   Embedded Systems", 25th IEEE Real-Time and Embedded Technology and
   Applications Symposium (RTAS'19). To appear.

3. I. SaÃ±udo, P. Cortimiglia, L. Miccio, M. Solieri, P. Burgio, C. di Biagio, F.
   Felici, G. Nuzzo, M. Bertogna. 2020. "The Key Role of Memory in
   Next-Generation Embedded Systems for Military Applications", in: Ciancarini
   P., Mazzara M., Messina A., Sillitti A., Succi G. (eds) Proceedings of 6th
   International Conference in Software Engineering for Defence Applications.
   SEDA 2018. Advances in Intelligent Systems and Computing, vol 925. Springer,
   Cham.

4. G. Gracioli, A. Alhammad, R. Mancuso, A.A. FrÃ¶hlich, and R. Pellizzoni. 2015.
   "A Survey on Cache Management Mechanisms for Real-Time Embedded Systems", ACM
   Comput. Surv. 48, 2, Article 32 (Nov. 2015), 36 pages. DOI:10.1145/2830555




Usage
-----

### Enable coloring support in Jailhouse

In order to compile Jailhouse with coloring support add `CONFIG_COLORING` to the
hypervisor configuration file (include/jailhouse/config.h)
```
#define CONFIG_COLORING 1
```

### Colors selection using indices

We shall first explain how to properly choose a color assignment for a given
software system.
Secondly, we are going to deep into the root cell configuration, which enables
cache coloring support for inmates.
Lastly, we are explaining how a color selection can be assigned to a given cell
configuration.

In order to choose a color assignment for a set of inmates, the first thing we
need to know is... the available color set. The number of available colors can
be either calculated[^1] or read from the handy output given by Jailhouse once
we enable the hypervisor.

```
...
Max number of avail. colors: 16
Page pool usage after early setup: mem 39/992, remap 0/131072
...
```

[^1]: To compute the number of available colors on the platform one can simply
  divide
  `way_size` by `page_size`, where: `page_size` is the size of the page used
  on the system (usually 4 KiB); `way_size` is size of a LLC way, i.e. the same
  value that has to be provided in the root cell configuration.
  E.g., 16 colors on a platform with LLC ways sizing 64 KiB and 4 KiB pages.

Once the number of available colors (N) is known, we select a range of colors
between 0 and N-1 and we will use the `jailhouse_cache` structure to to inform
the hypervisor of our choice, as later explained in the section about
[cells configuration](#cells-configuration).

Ex:
```
Max. available colors 16 [0-15]
Range_0: [0-5]
Range_1: [8-15]
```

#### Partitioning

We can choose any kind of color configuration we want but in order to have
mutual cache protection between cells, different colors must be assigned to them.
Another point to remember is to keep colors as contiguous as possible, so to
allow caches to exploit the higher performance to central memory controller.
So using different but single ranges for each cells is the most simple way to
have mutual cache protection.\
Ex: cell-1 has range `[0-7]` and cell-2 has range `[8-15]`.

### Root Cell configuration

#### Load start address

Coloring support uses a special memory region to load binaries when cells are
configured to use cache coloring. This mapping has the same size and coloring
selection as the cell but it starts from a virtual address that is defined by
the variable `col_load_address`. This value is set to 16 GiB by default but it
can be configured by the user depending on the platform. The value should be
set to a memory space that is not used in the platform e.g., devices.

For example, if we want to change this address to 32 GiB:
```
...
.platform_info = {
    ...
    .col_load_address = 0x800000000,
    .arm = {
        .gic_version = 2,
        .gicd_base = 0xf9010000,
        .gicc_base = 0xf902f000,
        .gich_base = 0xf9040000,
...
```

#### LLC way size

This field is _mandatory_ for using coloring support. It can be explicitly
defined by the user in the Root Cell configuration or it can be leaved empty and
the hypervisor will try to probe the value.
The value corresponds to the way size in bytes of the Last Level Cache and
could be calulated by dividing the LLC size by the number of way
(the way number refers to N-way set-associativity).

For example, a 16-way set associative cache sizing 1 MiB has a way size of
64 KiB:
```
...
.platform_info = {
    ...
    .llc_way_size = 0x10000,
    .arm = {
        .gic_version = 2,
        .gicd_base = 0xf9010000,
        .gicc_base = 0xf902f000,
        .gich_base = 0xf9040000,
...
```

### Cells configuration

First of all we need to select one or more range(s) for coloring
configuration that will be applied to **all** memory regions flagged with
`JAILHOUSE_MEM_COLORED`. Each range is defined using the `struct jailhouse_cache`
where the `.start` and `.size` values correspond respectively the start and size
of the range itself.
```
...
struct jailhouse_memory mem_regions[12];
struct jailhouse_cache cache_regions[1];
...
.num_memory_regions = ARRAY_SIZE(config.mem_regions)
.num_cache_regions = ARRAY_SIZE(config.cache_regions),
...
.mem_regions = {
        ...
        /* Colored RAM */ {
                .phys_start = 0x810000000,
                .virt_start = 0x0,
                .size = 0x10000,
                .flags = JAILHOUSE_MEM_READ | JAILHOUSE_MEM_WRITE |
                        JAILHOUSE_MEM_EXECUTE | JAILHOUSE_MEM_DMA |
                        JAILHOUSE_MEM_LOADABLE| JAILHOUSE_MEM_COLORED,
        },
        ...
}
...
.cache_regions = {
        {
                .start = 0,
                .size = 8,
        },
},
...
```
In the example we have a platform with 16 colors available and we are selecting
the range [0-7] that goes from 0 to 7 (8 colors).

#### Overlaps and colored memory sizes

When using colored memory regions the rule `phys_end = phys_start + size` is no
longer true. So the configuration must be written carefully in order to avoid to
exceed the available memory in the root cell. The hypervisor performs a check
when a colored region is created but it's up to the configurator to fix the
issue.
Moreover, since the above rule does not apply, it is very common to have overlaps
between colored memory regions of different cells if they are sharing colors.
