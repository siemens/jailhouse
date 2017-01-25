Things to be addressed, at some point, or at least before releasing version 1.0
(tagged with [v1.0]). Otherwise unsorted, unprioritized, likely incomplete.

x86 support
  - AMD interrupt remapping support
  - power management [v1.0]
    - block
    - allow per cell (managing inter-core/inter-cell impacts)
  - NMI control/status port - moderation or emulation required? [v1.0]
  - whitelist-based MSR access [v1.0]
  - CAT enhancements
    - add support for CDP (code/data L3 partitioning)
    - add support for L2 partitioning (-> Apollo Lake), including accurate
      modeling of the partitioning scope (affected CPUs)

ARM support
  - v7 (32-bit)
    - analyze cp15 system control registers access, trap critical ones
  - v8 (64-bit)
    - check if we need arch_inject_dabt
    - analyze system constrol registers access, specifically regarding cache
      maintenance and side effects on neighboring cores
    - GICv3 support
  - common (v7 and v8)
    - System MMU support
    - runtime selection of GICv2 vs. v3
    - re-evaluate IRQ priorities for GIC emulation and possibly add support
    - properly reset interrupts on cell reset or reassignment
    - support for big endian? (depends on relevant targets)
      - infrastructure to support BE architectures (byte-swapping services)
      - usage of that infrastructure in generic subsystems
      - specific BE support for first target

Configuration
 - review of format, rework of textual representation
 - refactor config generator
    - better internal structure, also to prepare non-x86 support
    - move into Python module, for reuse by multiple helper scripts
 - enhance config generator
    - confine the created root cell config to the essentially required
      resources (e.g. PCI BARs)
    - generate non-root cell configs
    - add knowledge base about resource access rules that need manual review or
      configurations that are known to be problematic (e.g. INTx sharing
      between cells)

Setup validation
  - check integrity of configurations
  - check integrity of runtime environment (hypervisor core & page_pool,
    probably just excluding volatile Linux-related state variables)
    - pure software solution (without security requirements)
    - Intel TXT support? [WIP: master thesis]
    - secure boot?
  - check for execution inside hypervisor, allow only when enabled in config
  - clear memory regions before reassignment to prevent information leaks?

Inter-cell communication
  - finalize and specify shared memory device [v1.0]
    - 3 types of regions (r/w both, r/w local, r/o local)
    - unprivileged MMIO register region (UIO-suitable)
    - fast-path for checking remote state (vmexit-free)
    - clarify: "ivshmem 2.0" or own device (with own IDs)
  - specify virtual Ethernet protocol [v1.0]
  - specify and implements virtual console protocol
  - upstream Linux drivers

Testing
  - unit tests
  - system tests, also in QEMU/KVM, maybe using Lava + Fuego

Inmates
  - reusable runtime environment for cell inmates
    - skeleton in separate directory
    - inter-cell communication library
  - port free small-footprint RTOS to Jailhouse bare-metal environment
    - RTEMS upstream support
    - Zephyr?
  - upstream Linux support
    - x86 patches
    - removable generic PCI host controller
    - discuss remaining patches (export __boot_cpu_mode, CONFIG_PCI_DOMAINS)

Hardware error handling
  - MCE processing + managed forwarding [v1.0]
  - PCI AER
  - APEI
  - Thermal
  - ...

Monitoring
  - report error-triggering devices behind IOMMUs via sysfs
  - cell software watchdog via comm region messages
    -> time out pending comm region messages and kill failing cells
       (includes timeouts of unanswered shutdown requests)
