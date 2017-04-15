VGA console
===========

The Jailhouse hypervisor uses the printk and panic_printk functions for
printing messages. Both functions eventually call an architecture-dependent
function named 'arch_dbg_write()' which actually prints the message out
through a configurable hardware device. Typically, this hardware device
is a serial port.

Although most ARM boards come with at least one serial port, modern x86
computers often lack it. In order to address such scenarios, the VGA console
feature provides an alternative debugging method for x86 computers based on
the VGA text mode buffer.


Usage
-----

Add the following to the header section of your root cell's config:

    .debug_console = {
        .address = 0xb8000,
        .size = 0x1000,
        .flags = JAILHOUSE_CON1_TYPE_VGA | JAILHOUSE_CON1_ACCESS_MMIO,
    },

Boot using the following additional kernel parameters:

    vga=normal nofb video=vesafb:off nomodeset i915.modeset=0

Load the jailhouse kernel module. Use the 'vbetool' command to set the
current VESA mode and enable your root cell.

    # modprobe jailhouse
    # vbetool vbemode set 3 && jailhouse enable configs/system.cell

[Note] for testing on QEMU replace 'system.cell' by 'qemu-vm.cell'.


References
----------

- https://en.wikipedia.org/wiki/VGA-compatible_text_mode
- http://wiki.osdev.org/Text_UI
