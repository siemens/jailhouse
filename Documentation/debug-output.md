Jailhouse Debug Output
======================

System Configuration
--------------------

Jailhouse supports various debug output drivers.  The debug output of the
hypervisor is selected in the system configuration inside the debug_console
structure.  The 'type' member selects the output driver and the 'flags' member
specifies additional options.

### .type and .flags
All architectures support the empty debug output driver, which is selected by
default if nothing else is chosen:

    - JAILHOUSE_CON_TYPE_NONE

Possible debug outputs for x86:

    - JAILHOUSE_CON_TYPE_8250      /* 8250-compatible UART (PIO or MMIO) */
    - JAILHOUSE_CON_TYPE_EFIFB     /* EFI framebuffer console */

Possible debug outputs for arm and arm64:

    - JAILHOUSE_CON_TYPE_8250      /* 8250 compatible UART*/
    - JAILHOUSE_CON_TYPE_PL011     /* AMBA PL011 UART */
    - JAILHOUSE_CON_TYPE_XUARTPS   /* Xilinx UART */
    - JAILHOUSE_CON_TYPE_MVEBU     /* Marvell UART */
    - JAILHOUSE_CON_TYPE_HSCIF     /* Renesas HSCIF UART */
    - JAILHOUSE_CON_TYPE_SCIFA     /* Renesas SCIFA UART */
    - JAILHOUSE_CON_TYPE_SCIF      /* Renesas SCIF UART */
    - JAILHOUSE_CON_TYPE_IMX       /* NXP i.MX UART */
    - JAILHOUSE_CON_TYPE_IMX_LPUART/* NXP i.MX LPUART */

Possible access modes, to be or'ed:

    - JAILHOUSE_CON_ACCESS_PIO     /* PIO, x86 only */
    - JAILHOUSE_CON_ACCESS_MMIO    /* MMIO, x86 and ARM */

Possible register distances (MMIO only, PIO is implicitly 1-byte), to be or'ed:

    - JAILHOUSE_CON_REGDIST_1      /* 1-byte distance */
    - JAILHOUSE_CON_REGDIST_4      /* 4-bytes distance */

Possible framebuffer formats (EFIFB only);

    - JAILHOUSE_CON_FB_1024x768    /* 1024x768 pixel, 32 bit each */
    - JAILHOUSE_CON_FB_1920x1080   /* 1920x1080 pixel, 32 bit each */

### .address and .size
The address member denotes the base address of the Debug console (PIO or MMIO
base address). The .size parameter is only required for MMIO.

### .divider
An optional UART divider parameter that can be passed to the driver. This is
supported by the 8250 driver.

A zero value means that the hypervisor or the inmate will skip the
initialisation of the UART console.  This is the case in most scenarios, as the
hypervisor's UART console was initialised by Linux before.

Defaults to 0.

### .clock_reg and .gate_nr
If Linux does not initialise the UARTs, Jailhouse has to initialise them on
its own.  Some UARTs require to gate a clock before the UART can be used.

Ignored if "clock_reg" is 0, both default to 0.

Clock gating is currently only supported on 32-bit ARM.

### Examples
Example configuration for PIO based debug output on x86:

    .debug_console = {
        .address = 0x3f8, /* PIO address */
        .divider = 0x1, /* 115200 Baud */
        .type = JAILHOUSE_CON_TYPE_8250, /* choose the 8250 driver */
        .flags = JAILHOUSE_CON_PIO,      /* chose PIO register access */
    },

Example configuration for MMIO based debug output on ARM (8250 UART):

    .debug_console = {
        .address = 0x70006300, /* MMIO base address */
        .size = 0x40, /* size */
        .clock_reg = 0x60006000 + 0x330, /* Optional: Debug Clock Register */
        .gate_nr = (65 % 32), /* Optional: Debug Clock Gate Nr */
        .divider = 0xdd, /* 115200 */
        .type = JAILHOUSE_CON_TYPE_8250, /* choose the 8250 driver */
        .flags = JAILHOUSE_CON_MMIO_32,  /* choose 32-bit MMIO access */
    },

Example configuration for EFI framebuffer debug out on x86:

    .debug_console = {
        .address = 0x80000000, /* framebuffer base address */
        .size = 0x300000, /* 1024x768x4 */
        .type = JAILHOUSE_CON_TYPE_EFIFB,  /* choose the EFIFB driver */
        .flags = JAILHOUSE_CON_MMIO | \    /* access is MMIO */
                 JAILHOUSE_CON_FB_1024x768 /* format */
    },

Example configuration for disabled debug output (architecture independent):

    .debug_console = {
        .flags = JAILHOUSE_CON_TYPE_NONE,
    }


Jailhouse Virtual Console
-------------------------

If the system configuration has the flag JAILHOUSE_SYS_VIRTUAL_DEBUG_CONSOLE
set, the hypervisor console is available through
/sys/devices/jailhouse/console.  Continuous reading of the hypervisor console
is available through /dev/jailhouse.

Example

    cat /dev/jailhouse
 or
    jailhouse console -f

If a cell configuration of a non-root cells has the flag
JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED set, the inmate is allowed to use the
dbg_putc hypercall to write to the hypervisor console. This is useful for
debugging, as the root cell is able to read the output of the inmate.

The flag JAILHOUSE_CELL_VIRTUAL_CONSOLE_ACTIVE implies
JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED and shall cause the inmate to
automatically use the virtual console as an output path.


Jailhouse Inmates
-----------------

As well as the hypervisor, inmates choose their output driver during runtime.
By default, the particular driver is chosen by the "console" field in the
non-root cell configuration. This field is passed to the inmate via the
communication region.  If a non-root cell has the flag
JAILHOUSE_CELL_VIRTUAL_CONSOLE_ACTIVE set, Jailhouse inmates will additionally
write to the Jailhouse virtual console.

The default remains off, and the administrator is expected to grant this
permission only temporarily while debugging a specific cell.  Note that output
might be duplicated, if the hypervisor shares the console with the inmate.

The "console" parameters of the cell's configration may always be overrided by
inmate command line parameters.

### Parameter list
| Parameter     | Description                   | x86                | ARM and ARM64                   |
|:--------------|:------------------------------|:-------------------|:--------------------------------|
| con-type      | Primary Debug Output Driver   | see below          | see below                       |
| con-base      | PIO/MMIO Base Address         | e.g. 0x3f8         | e.g. 0x70006000                 |
| con-divider   | UART divider                  | 0x1                | 0x0d                            |
| con-clock-reg | Clock Register                | not supported      |                                 |
| con-gate-nr   | Clock Gate Nr                 | not supported      |                                 |
| con-regdist-1 | MMIO: 8 bit register distance | true / false       | true / false                    |
| con-is-mmio   | MMIO'ed access mode           | true / false       | not supported, ARM is MMIO only |
| con-virtual   | Use secondary virtual console | true / false       | true / false                    |

Available debug output drivers (con-type=):
x86: none, 8250
arm: none, 8250, hscif, imx, mvebu, pl011, scifa, xuartps

Similar to the hypervisor configuration, a zero value for con-divider will skip
initialisation of the UART interface.

On x86, EFI framebuffer output is not available for inmates.

### Examples
Example command line parameters for PIO based debug output on x86, where the
inmate will initialise UART:

    jailhouse cell load foocell inmate.bin \
        -s "con-base=0x3f8 con-is-mmio=false con-divider=1" -a 0x1000

Example configuration for MMIO based debug output on ARM using the 8250 driver:

    jailhouse cell load foocell inmate.bin \
        -s "con-type=8250 con-is-mmio=true con-base=0x70006000 con-divider=0xdd" -a 0x1000

Example configuration for MMIO based debug output on ARM64 using the PL011 driver:

    jailhouse cell load foocell inmate.bin \
        -s "con-type=PL011 con-is-mmio=true con-base=0xf7113000" -a 0x1000
