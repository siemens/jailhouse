Setup on ZCU102 ZynqMP Ultrascale+
==================================
The ZCU102 target is a Xilinx target based on ZynqMP Ultrascale+. The SoC is a
quad-core Cortex-A53 and a dual-core R5 real-time processor. Further
information can be found on
https://www.xilinx.com/products/boards-and-kits/ek-u1-zcu102-g.html.

The Linux Image which runs Jailhouse has been built with Petalinux 2017.4.
Petalinux uses linux-xlnx repository, and in this case it uses the
xilinx-v2017.4 one, which is based on 4.9 kernel.


Image build
-----------
In order to build the Linux image with Petalinux it is necessary to set all the
environmental variables. 

    $ source /opt/pkg/settings.sh

Once petalinux environments are set, the Petalinux project is created with the
name lnx_jailhouse. The bsp has to be downloaded.

    $ petalinux-create -t project --template zynqMP -s ../xilinx-zcu102-v2017.4-final.bsp -n lnx_jailhouse

The Linux project is configured by:

    $ petalinux-config 

A menuconfig window is opened and just enable `Root filesystem type (SD card)`:

    Image Packaging Configuration--->Root filesystem type-->SD card

Save project and exit. It will take some to time to configure the project. Once
it has finished configuring, the Linux kenel needs to be configured enabling
`CONFIG_OF_OVERLAY` and `CONFIG_KALLSYMS_ALL`

    $ petalinux-config -c kernel

Once modified, save the changes and build the project.

    $ petalinux-build
    $ petalinux-package --boot --u-boot


Jailhouse build
---------------
In the Jailhouse source directory, create a file include/jailhouse/config.h
with the following lines:

#define CONFIG_MACH_ZYNQMP_ZCU102      1

For debugging and for obtaining more information it is also possible to add:
#define CONFIG_TRACE_ERROR             1


After that, it is possible to build the project. It is necessary to set the
variables `ARCH=` with arm64, `KDIR=` with the kernel directory inside the
Petalinux project, `CROSS_COMPILE=` with the compiler and `DESTDIR=` with the
rootfs directory.

    $ make ARCH=arm64 CROSS_COMPILE=aarch64-linux-gnu- KDIR=../lnx_jailhouse/build/tmp/work/plnx_aarch64-xilinx-linux/linux-xlnx/4.9-xilinx-v2017.4+gitAUTOINC+b450e900fd-r0/linux-plnx_aarch64-standard-build/ DESTDIR=/media/user/rootfs install

This command will add the jailhouse module and its binary.


U-Boot
------
Jailhouse needs the Linux kernel boot parameters `mem=` to be set in order to
reserve memory for other cells. In this case we chose `mem=1536M`. This can be
done through U-Boot:

    ZynqMP> setenv bootargs "earlycon clk_ignore_unused earlyprintk mem=1536M root=/dev/mmcblk0p2 rw rootwait"
    ZynqMP> setenv uenvcmd "fatload mmc 0 0x3000000 Image && fatload mmc 0 0x2A00000 system.dtb && booti 0x3000000 - 0x2A00000"
    ZynqMP> setenv bootcmd "run uenvcmd"
    ZynqMP> saveenv
    ZynqMP> boot


Testing Jailhouse GIC Demo
--------------------------
Copy the `configs/arm64/zynqmp-zcu102*.cell` to the rootfs. To test Jailhouse
it is also interesting to copy `inmates/demos/arm64/gic-demo.bin`. This demo
creates a periodic timer interrupt and calculates its jitter. Once Linux is
running:

    # modprobe jailhouse
    [   24.309597] jailhouse: loading out-of-tree module taints kernel.

    # jailhouse enable zynqmp-zcu102.cell
    Initializing Jailhouse hypervisor v0.8 on CPU 2
    Code location: 0x0000ffffc0200060
    Page pool usage after early setup: mem 33/993, remap 64/131072
    Initializing processors:
     CPU 2... OK
     CPU 0... OK
     CPU 3... OK
     CPU 1... OK
    Initializing unit: irqchip
    Initializing unit: PCI
    Adding virtual PCI device 00:00.0 to cell "ZynqMP-ZCU102"
    Adding virtual PCI device 00:01.0 to cell "ZynqMP-ZCU102"
    Page pool usage after late setup: mem 42/993, remap 69/131072
    Activating hypervisor
    [   39.844953] The Jailhouse is opening.

    # jailhouse cell create zynqmp-zcu102-inmate-demo.cell
    [   55.351670] CPU3: shutdown
    [   55.354303] psci: CPU3 killed.
    Created cell "inmate-demo"
    Page pool usage after cell creation: mem 56/993, remap 69/131072
    [   55.388029] Created Jailhouse cell "inmate-demo"

    # jailhouse cell load 1 gic-demo.bin
    Cell "inmate-demo" can be loaded
    # jailhouse cell start 1

Second UART starts showing jitter data:

    Timer fired, jitter:   2212 ns, min:   2101 ns, max:   2585 ns
    Timer fired, jitter:   2171 ns, min:   2101 ns, max:   2585 ns
    Timer fired, jitter:   2454 ns, min:   2101 ns, max:   2585 ns
    Timer fired, jitter:   2444 ns, min:   2101 ns, max:   2585 ns
    Timer fired, jitter:   2181 ns, min:   2101 ns, max:   2585 ns
    Timer fired, jitter:   2181 ns, min:   2101 ns, max:   2585 ns
    Timer fired, jitter:   2212 ns, min:   2101 ns, max:   2585 ns

If second UART does not show anything, the problem can be between the DTB and
Jailhouse. Jailhouse is not still compatible with the last Xilinx DTBs.
Therefore, it is recommendable to use a 2016 repository DTB, such as,
https://github.com/Xilinx/linux-xlnx/blob/xilinx-v2016.4/arch/arm64/boot/dts/xilinx/zynqmp-zcu102.dts.


Testing Jailhouse Linux
-----------------------
It is possible to load a Linux image in a guest cell. In order to do that, it
is essential to do a new Linux image but with
`Image Packaging Configuration--->Root filesystem type-->INITRAMFS`.

The files `image/linux/Image` and `image/linux/rootfs.cpio` have to be copied 
from the Petalinux project to the rootfs. Besides,
`configs/arm64/dts/inmate-zynqmp.dtb` file has to be copied from Jailhouse
project.

To load Linux in the guest cell:

    # modprobe jailhouse
    [   24.309597] jailhouse: loading out-of-tree module taints kernel.

    # jailhouse enable zynqmp-zcu102.cell
    Initializing Jailhouse hypervisor v0.8 on CPU 2
    Code location: 0x0000ffffc0200060
    Page pool usage after early setup: mem 33/993, remap 64/131072
    Initializing processors:
     CPU 2... OK
     CPU 0... OK
     CPU 3... OK
     CPU 1... OK
    Initializing unit: irqchip
    Initializing unit: PCI
    Adding virtual PCI device 00:00.0 to cell "ZynqMP-ZCU102"
    Adding virtual PCI device 00:01.0 to cell "ZynqMP-ZCU102"
    Page pool usage after late setup: mem 42/993, remap 69/131072
    Activating hypervisor
    [   39.844953] The Jailhouse is opening.

    # jailhouse cell linux zynqmp-zcu102-linux-demo.cell Image -d inmate-zynqmp.dtb -i rootfs.cpio -c "console=ttyPS0,115200"
    [   81.967652] CPU2: shutdown
    [   81.970285] psci: CPU2 killed.
    [   82.015619] CPU3: shutdown
    [   82.018242] psci: CPU3 killed.
    Adding virtual PCI device 00:00.0 to cell "ZynqMP-linux-demo"
    Shared memory connection established: "ZynqMP-linux-demo" <--> "ZynqMP-ZCU102"
    Adding virtual PCI device 00:02.0 to cell "ZynqMP-linux-demo"
    Created cell "ZynqMP-linux-demo"
    Page pool usage after cell creation: mem 61/993, remap 69/131072
    [   82.062667] Created Jailhouse cell "ZynqMP-linux-demo"
    Cell "ZynqMP-linux-demo" can be loaded
    Started cell "ZynqMP-linux-demo"

Guest Linux booting will appear in the second UART. It possible to see the
state of each cell:

    root@xilinx-zcu102-2017_4:/# jailhouse cell list
    ID      Name                    State             Assigned CPUs           Failed CPUs
    0       ZynqMP-ZCU102           running           0-1
    1       ZynqMP-linux-demo       running           2-3


Debug in ZCU102
---------------
The debug console is supposed to help with early cell boot issues and/or when a
UART is not available or not working. Add `JAILHOUSE_CELL_VIRTUAL_CONSOLE_PERMITTED`
to the cell config (see e.g. configs/arm64/qemu-arm64-linux-demo.c).

As an example, the gic-demo can be debugged. Just add the following info in the
cell load. It will print all the data through the principal UART instead of from
the second one.

    # jailhouse cell load 1 gic-demo.bin -s "con-type=JAILHOUSE" -a 0x1000

The output can be seen in:

    # jailhouse console -f
