Hypervisor Configuration
========================

Jailhouse supports various static compile-time configuration
parameters, such as platform specific settings and debugging options.
Those settings can optionally be defined in
'hypervisor/include/jailhouse/config.h'.
Every configuration option should be defined to "1" or not be in the file at
all. Defining any other value can cause unexpected behaviour.

Available configuration options
-------------------------------

General configuration parameters

    /* Print error sources with filename and line number to debug console */
    #define CONFIG_TRACE_ERROR 1

    /*
     * Set instruction pointer to 0 if cell CPU has caused an access violation.
     * Linux inmates will dump a stack trace in this case.
     */
    #define CONFIG_CRASH_CELL_ON_PANIC 1

    /* Enable code coverage data collection (see Documentation/gcov.txt) */
    #define CONFIG_JAILHOUSE_GCOV 1

    /*
     * Link inmates against a custom base address.  Only supported on ARM
     * architectures.  If this parameter is defined, inmates must be loaded to
     * the appropriate location.
     */
    #define CONFIG_INMATE_BASE 0x90000000

    /*
     * Strip Jailhouse specific parts from inmates (e.g., heartbeat()).  This
     * allows inmates to be booted on bare-metal, without Jailhouse and is
     * mainly used for testing purposes.
     *
     * See configs/jetson-tk1-demo.c for the usage of this parameter in cell
     * configurations.
     */
    #define CONFIG_BARE_METAL 1

### Example board specific configurations

#### ARM

##### BananaPi M1

    #define CONFIG_MACH_BANANAPI 1
    #define CONFIG_ARM_GIC_V2 1

##### Nvidia Jetson TK1

    #define CONFIG_MACH_JETSON_TK1 1
    #define CONFIG_ARM_GIC_V2 1

##### Xunlong Orange Pi Zero, 256 MiB

    #define CONFIG_MACH_ORANGEPI0 1
    #define CONFIG_ARM_GIC_V2 1

##### ARM Fast Model

    #define CONFIG_MACH_VEXPRESS 1
    /* Fast Model supports both, GICv2 and GICv3 */
    #define CONFIG_ARM_GIC_V2 1
    /* #define CONFIG_ARM_GIC_V3 */

#### ARM64

##### Nvidia Jetson TK1

    #define CONFIG_MACH_JETSON_TX1 1
    #define CONFIG_ARM_GIC_V2 1

##### Xilinx Zynq UltraScale+ MPSoC ZCU102

    #define CONFIG_MACH_ZYNQMP_ZCU102 1
    #define CONFIG_ARM_GIC_V2 1

##### HiKey LeMaker 2 GiB

    #define CONFIG_MACH_HIKEY 1
    #define CONFIG_ARM_GIC_V2 1

##### ARMv8 Foundation Model

    #define CONFIG_MACH_FOUNDATION_V8 1
    #define CONFIG_ARM_GIC_V2 1

##### AMD ARM-Opteron A1100

    #define CONFIG_MACH_AMD_SEATTLE 1
    #define CONFIG_ARM_GIC_V2 1
