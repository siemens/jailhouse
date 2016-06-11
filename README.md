JAILHOUSE
=========

Jailhouse is a partitioning Hypervisor based on Linux. It is able to run
bare-metal applications or (adapted) operating systems besides Linux. For this
purpose it configures CPU and device virtualization features of the hardware
platform in a way that none of these domains, called "cells" here, can
interfere with each other in an unacceptable way.

Jailhouse is optimized for simplicity rather than feature richness. Unlike
full-featured Linux-based hypervisors like KVM or Xen, Jailhouse does not
support overcommitment of resources like CPUs, RAM or devices. It performs no
scheduling and only virtualizes those resources in software, that are essential
for a platform and cannot be partitioned in hardware.

Once Jailhouse is activated, it runs bare-metal, i.e. it takes full control
over the hardware and needs no external support. However, in contrast to other
bare-metal hypervisors, it is loaded and configured by a normal Linux system.
Its management interface is based on Linux infrastructure. So you boot Linux
first, then you enable Jailhouse and finally you split off parts of the
system's resources and assign them to additional cells.


WARNING: This is work in progress! Don't expect things to be complete in any
dimension. Use at your own risk. And keep the reset button in reach.


Community Resources
-------------------

Project home:

 - https://github.com/siemens/jailhouse

Source code:

 - https://github.com/siemens/jailhouse.git
 - git@github.com:siemens/jailhouse.git

Frequently Asked Questions (FAQ):

 - See [FAQ file](FAQ.md)

Mailing list:

  - jailhouse-dev@googlegroups.com

  - Subscription:
    - jailhouse-dev+subscribe@googlegroups.com
    - https://groups.google.com/forum/#!forum/jailhouse-dev/join

  - Archives
    - http://news.gmane.org/gmane.linux.jailhouse

Continuous integration:

  - https://travis-ci.org/siemens/jailhouse

  - Status:
    - ![](https://travis-ci.org/siemens/jailhouse.svg?branch=master) on master
    - ![](https://travis-ci.org/siemens/jailhouse.svg?branch=next) on next

Static code analysis:

  - https://scan.coverity.com/projects/4114

  - Status:
    - ![](https://scan.coverity.com/projects/4114/badge.svg) on coverity_scan

See the [contribution documentation](CONTRIBUTING.md) for details
on how to write Jailhouse patches and propose them for upstream integration.


Requirements (preliminary)
--------------------------

x86 architecture:

  - Intel system:

    - support for 64-bit and VMX, more precisely
      - EPT (extended page tables)
      - unrestricted guest mode
      - preemption timer

    - Intel IOMMU (VT-d) with interrupt remapping support
      (except when running inside QEMU)

  - or AMD system:

    - support for 64-bit and SVM (AMD-V), and also
      - NPT (nested page tables); required
      - Decode Assists; recommended

    - AMD IOMMU (AMD-Vi) is unsupported now but will be required in future

  - at least 2 logical CPUs

  - x86-64 Linux kernel (tested against >= 3.14)

    - VT-d IOMMU usage (DMAR) has to be disabled in the Linux kernel, e.g. via
      the command line parameter:

          intel_iommu=off

    - To exploit the faster x2APIC, interrupt remapping needs to be on in the
      kernel (check for CONFIG_IRQ_REMAP)

ARM architecture:

  - Abstract:

    - ARMv7 with virtualization extensions

    - Appropriate boot loader support (typically U-Boot)
      - Linux is started in HYP mode
      - PSCI support for CPU offlining

    - at least 2 logical CPUs

  - Board support:

    - Banana Pi ([see more](Documentation/setup-on-banana-pi-arm-board.md))

    - NVIDIA Jetson TK1

    - ARM Versatile Express with Cortex-A15 or A7 cores
      (includes ARM Fast Model)

On x86, hardware capabilities can be validated by running

    jailhouse hardware check sysconfig.cell

using the binary system configuration created for the target (see
[below](#configuration)).


Build & Installation
--------------------

Simply run make, optionally specifying the target kernel directory:

    make [KDIR=/path/to/kernel/objects]

Except for the hypervisor image `jailhouse*.bin` that has to be available in the
firmware search path (invoke `make firmware_install` for this), you can run
Jailhouse from the build directory. Alternatively, install everything on the
target machine by calling `make install` from the top-level directory.


Configuration
-------------

Jailhouse requires one configuration file for the complete system and one for
each additional cell besides the primary Linux. These .cell files have to be
passed to the jailhouse command line tool for enabling the hypervisor or
creating new cells.

A system configuration can be created on the target system by running the
following command:

    jailhouse config create sysconfig.c

In order to translate this into the required binary form, place this file in
the configs/ directory. The build system will pick up every .c file from there
and generate a corresponding .cell file.

Depending on the target system, the C structures may require some adjustments to
make Jailhouse work properly or to reduce the desired access rights of the Linux
root cell.

Configurations for additional (non-root) cells currently require manual
creation. To study the structures, use one of the demo cell configurations files
as reference, e.g. configs/apic-demo.c or configs/e1000-demo.c.


Demonstration in QEMU/KVM
-------------------------

The included system configuration qemu-vm.c can be used to run Jailhouse in
QEMU/KVM virtual machine on x86 hosts (Intel and AMD are supported). Currently
it requires Linux 3.18 or newer on the host side (Intel is fine with 3.17).
QEMU is required in a recent version (2.1) as well if you want to use the
configuration file included in the source tree.

You also need a Linux guest image with a recent kernel (tested with >= 3.9) and
the ability to build a module for this kernel. Further steps depend on the type
of CPU you have on your system.

For Intel CPUs: Make sure the kvm-intel module was loaded with nested=1 to
enable nested VMX support. Start the virtual machine as follows:

    qemu-system-x86_64 -machine q35 -m 1G -enable-kvm -smp 4 \
        -cpu kvm64,-kvm_pv_eoi,-kvm_steal_time,-kvm_asyncpf,-kvmclock,+vmx \
        -drive file=LinuxInstallation.img,format=raw|qcow2|...,id=disk,if=none \
        -device ide-hd,drive=disk -serial stdio -serial vc \
        -device intel-hda,addr=1b.0 -device hda-duplex

For AMD CPUs: Make sure the kvm-amd module was loaded with nested=1 to enable
nested SVM support. Start the virtual machine as follows:

    qemu-system-x86_64 -machine q35 -m 1G -enable-kvm -smp 4 \
        -cpu host,-kvm_pv_eoi,-kvm_steal_time,-kvm_asyncpf,-kvmclock \
        -drive file=LinuxInstallation.img,format=raw|qcow2|...,id=disk,if=none \
        -device ide-hd,drive=disk -serial stdio -serial vc \
        -device intel-hda,addr=1b.0 -device hda-duplex

Inside the VM, make sure that `jailhouse-*.bin`, generated by the build process,
are available for firmware loading (typically /lib/firmware), see above for
installation steps.

The hypervisor requires a contiguous piece of RAM for itself and each
additional cell. This currently has to be pre-allocated during boot-up. So you
need to add

    memmap=66M$0x3b000000

as parameter to the command line of the virtual machine's kernel. The Jailhouse
QEMU cell config will block use of the serial port by the guest OS, so make
sure that the guest kernel command line does NOT have its console set to log
to the serial port (ie remove any 'console=ttyS0' arguments from the grub
config). Reboot the guest and load jailhouse.ko. Then enable Jailhouse like
this:

    jailhouse enable /path/to/qemu-vm.cell

Next you can create a cell with a demonstration application as follows:

    jailhouse cell create /path/to/apic-demo.cell
    jailhouse cell load apic-demo /path/to/apic-demo.bin -a 0xf0000
    jailhouse cell start apic-demo

apic-demo.bin is left by the built process in the inmates/demos/x86 directory.
This application will program the APIC timer interrupt to fire at 10 Hz,
measuring the jitter against the PM timer and displaying the result on the
console. Given that this demonstration runs in a virtual machine, obviously
no decent latencies should be expected.

After creation, cells are addressed via the command line tool by providing
their names or their runtime-assigned IDs. You can obtain information about
active cells this way:

    jailhouse cell list

Cell destruction is performed by specifying the configuration file of the
desired cell. This command will destroy the apic-demo:

    jailhouse cell destroy apic-demo

Note that the first destruction or shutdown request on the apic-demo cell will
fail. The reason is that this cell contains logic to demonstrate an ordered
shutdown as well as the ability of a cell to reject shutdown requests.

The apic-demo cell has another special property for demonstration purposes: As
long as it is running, no cell reconfigurations can be performed - the
apic-demo locks the hypervisor in this regard. In order to destroy another cell
or create an additional one, shut down the apic-demo first.

    jailhouse cell shutdown apic-demo  # call again if error is returned

To demonstrate the execution of a second, non-Linux cell, issue the following
commands:

    jailhouse cell create /path/to/pci-demo.cell
    jailhouse cell load pci-demo /path/to/pci-demo.bin -a 0xf0000
    jailhouse cell start pci-demo

The pci-demo will use the second serial port provided by QEMU. You will find
its output in a virtual console of the QEMU window. The purpose of this demo is
to show basic PCI device configuration and MSI handling.

While cell configurations are locked, it is still possible, though, to reload
the content of existing cell (provided they accept their shutdown first). To
reload and restart the tiny-demo, issue the following commands:

    jailhouse cell start apic-demo
    jailhouse cell load pci-demo /path/to/pci-demo.bin -a 0xf0000
    jailhouse cell start pci-demo

Finally, Jailhouse is can be stopped completely again:

    jailhouse disable  # call again on error due to running apic-demo

All non-Linux cells running at that point will be destroyed, and resources
will be returned to Linux.


