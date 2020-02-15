Hypervisor Configuration
========================

Jailhouse supports various static compile-time configuration
parameters, such as platform specific settings and debugging options.
Those settings can optionally be defined in
'include/jailhouse/config.h'.
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
     * Only available on x86. This debugging option that needs to be activated
     * when running mmio-access tests.
     */
    #define CONFIG_TEST_DEVICE 1
