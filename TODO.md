Things to be addressed, at some point. Unsorted, unprioritized, incomplete.

x86 support
  - AMD IOMMU support [WIP]
  - power management
    - block
    - allow per cell (managing inter-core/inter-cell impacts)
  - NMI control/status port - moderation or emulation required?
  - whitelist-based MSR access
  - add support for CDP (code/data L3 partitioning)

ARM support
  - v7 (32-bit) [WIP]
    - System MMU support
    - improve support for platform variations (device tree?)
  - v8 (64-bit)
  - support for big endian
    - infrastructure to support BE architectures (byte-swapping services)
    - usage of that infrastructure in generic subsystems
    - specific BE support for ARMv7, then v8

Configuration
 - review of format, rework of textual representation
 - platform device assignment
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

Inter-cell communication channel
  - analysis of virtio reuse
  - analysis of ARINC 653 semantics
  - high-level mechanisms (specifically queues) based on selected/modified
    standard
  - Linux for consoles and message-based interfaces (if not reusable)

Testing
  - unit tests?
  - system tests, also in QEMU/KVM
    - VT-d emulation for QEMU [WIP: interrupt redirection]

Inmates
  - reusable runtime environment for cell inmates
    - skeleton in separate directory
    - hw access libraries
      - x86: add TSC calibration
    - inter-cell communication library
  - port free small-footprint RTOS to Jailhouse bare-metal environment
    [WIP: RTEMS]

Hardware error handling
  - MCEs
  - PCI AER
  - APEI
  - Thermal
  - ...

Monitoring
  - report error-triggering devices behind IOMMUs via sysfs
  - hypervisor console via debugfs?
  - cell software watchdog via comm region messages  
    -> time out pending comm region messages and kill failing cells
       (includes timeouts of unanswered shutdown requests)

Misc
  - generic sub-page access filtering
    - use bitmap, likely with byte granularity, to filter access on specific
      registers in a MMIO page
  - generic and faster MMIO dispatching
    - use binary search on an per-cell array of (start, size, handler, opaque)
      entries
    - should be able to deal with both existing devices as well as sub-page
      filtering
