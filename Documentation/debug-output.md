Jailhouse Debug Output
======================

System Configuration
--------------------

Jailhouse supports various debug output drivers.  The debug output of the
hypervisor is selected in the system configuration inside the debug_output
structure.  The 'flags' member selects the output driver as well as additional
options.

### .flags
All architectures support the empty debug output driver, which is selected by
default if nothing else is chosen:
  - JAILHOUSE_CON_TYPE_NONE

Possible debug outputs for x86:
  - JAILHOUSE_CON_TYPE_UART_X86  /* generic X86 PIO/MMIO UART driver */
  - JAILHOUSE_CON_TYPE_VGA       /* VGA console */

VGA output is only available for x86. For further documentation on VGA output
see [vga-console.txt](vga-console.txt).

Possible debug outputs for arm and arm64:
  - JAILHOUSE_CON_TYPE_8250      /* 8250 compatible UART */
  - JAILHOUSE_CON_TYPE_PL011     /* AMBA PL011 UART */

Additional flags that can be or'ed:
  - JAILHOUSE_CON_FLAG_PIO   /* x86 only */
  - JAILHOUSE_CON_FLAG_MMIO  /* x86 and ARM. Should always be selected for
                              * ARM. */

### .address and .size
The address member denotes the base address of the Debug console (PIO or MMIO
base address). The .size parameter is only required for MMIO.

### .divider
An optional UART divider parameter that can be passed to the driver. This is
supported by the generic X86 UART driver and the 8250 driver.

A zero value means that the hypervisor will skip the initialisation of the UART
console.  This is the case in most scenarios, as the hypervisor's UART console
was initialised by Linux before.

Defaults to 0.

### .clock_reg and .gate_nr
If Linux does not initialise the UARTs, Jailhouse has to initialise them on
its own.  Some UARTs require a clock gate to be enabled before the UART can be
used.  Only the 8250 driver on ARM supports these parameters.  Ignored if 0.

Note that this feature is not yet supported by ARM64.

Both default to 0.

### Examples
Example configuration for PIO based debug output on x86:
```
.debug_console = {
	.address = 0x3f8, /* PIO address */
	.divider = 0x1, /* 115200 Baud */
	.flags = JAILHOUSE_CON_TYPE_UART_X86 | /* generic x86 UART driver */
		     JAILHOUSE_CON_FLAG_PIO, /* use PIO instead of MMIO */
},
```

Example configuration for MMIO based debug output on ARM (8250 UART):
```
.debug_console = {
	.address = 0x70006300, /* MMIO base address */
	.size = 0x40, /* size */
	.clock_reg = 0x60006000 + 0x330, /* Optional: Debug Clock Register */
	.gate_nr = (65 % 32), /* Optional: Debug Clock Gate Nr */
	.divider = 0xdd, /* 115200 */
	.flags = JAILHOUSE_CON_TYPE_8250 | /* choose the 8250 driver */
		     JAILHOUSE_CON_FLAG_MMIO,  /* choose MMIO register access */
},
```

Example configuration for disabled debug output (architecture independent):
```
.debug_console = {
	.flags = JAILHOUSE_CON_TYPE_NONE,
}
```

Inmates
-------

As well as the hypervisor, inmates choose their output driver during runtime.
The particular Driver is chosen by command line arguments.  If no arguments
are provided, inmates choose a default output driver.

On X86, default output driver is PIO UART on port 0x3f8, ARM devices choose
their output driver according to their settings in mach/debug.h.

### Parameter list
| Parameter     | Description                | X86        | ARM and ARM64   |
|---------------|:--------------------------:|-----------:|----------------:|
| con-type      | Debug Output Driver        | PIO, MMIO  | 8250, PL011     |
| con-base      | Base Address (PIO or MMIO) | e.g. 0x3f8 | e.g. 0x70006000 |
| con-divider   | UART divider               | 0x1        | 0x0d            |
| con-clock_reg | Clock Register             |            |                 |
| con-gate_nr   | Clock Gate Nr              |            |                 |

All architectures support the empty con-type "none".

Similar to the hypervisor configuration, a zero value for con-divider will skip
initialisation of the UART interface.

con-clock_reg and con-gate_nr are currently only available on ARM 8250 only.

On X86, VGA output is not available for inmates.

### Examples
Example command line parameters for PIO based debug output on x86, where the
inmate will initialise UART:
```
  jailhouse cell load foocell inmate.bin -a 0xf0000 \
    -s "con-base=0x3f8 con-divider=1" -a 0xf0000
```

Example configuration for MMIO based debug output on ARM using the 8250 driver:
```
  jailhouse cell load foocell inmate.bin -a 0x0 \
    -s "con-type=8250 con-base=0x70006000 con-divider=0xdd" -a 0x100
```

Example configuration for MMIO based debug output on ARM64 using the PL011 driver:
```
  jailhouse cell load foocell inmate.bin -a 0x0 \
    -s "con-type=PL011 con-base=0xf7113000" -a 0x1000
```
