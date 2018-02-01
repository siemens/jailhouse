Setup on emtrion's emCON-RZ/G1x series board
============================================

The emCON-RZ/G1x boards from emtrion are boards with SoCs from the Renesas RZ/G series:

- emCON-RZ/G1E has a RZ/G1E processor (dual-core Cortex-A7)
- emCON-RZ/G1M has a RZ/G1M processor (dual-core Cortex-A15)
- emCON-RZ/G1H has a RZ/G1H processor (quad-core Cortex-A15/Cortex-A7)

These boards run mainline Linux kernels and U-Boot with patches from Renesas and emtrion.
Further information can be found on https://www.emtrion.de and https://support.emtrion.de.

In order to run Jailhouse, the Linux kernel version should be at least 4.4.49 and U-Boot
should be at least 2016.07.

Adjusting kernel boot parameters via U-Boot
-------------------------------------------
Jailhouse needs the Linux kernel boot parameters mem= and vmalloc= to be set in order to reserve memory for other cells.
In our case we chose mem=750M and vmalloc=384M.

To set these values, you have to change the Linux kernel boot parameters via U-Boot accordingly.

Install and start Jailhouse on emCON-RZ/G1x
-------------------------------------------
First we need access to the RootFS of the Linux running on the emCON-RZ/G1x. We assume that you
have mounted this on your development workstation through sshfs or nfs.

Now in your Jailhouse source directory, create a file include/jailhouse/config.h.

If you own an emCON-RZ/G1E or emCON-RZ/G1M put the following line to this file:

#define CONFIG_MACH_EMCON_RZG		1

If you own an emCON-RZ/G1H put the following line to this file:

#define CONFIG_MACH_EMCON_RZG1H		1

Then you can compile and install Jailhouse using this command on the development workstation:

make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KERNELRELEASE=<kernel version>-emconrzg1x KDIR=<path to linux kernel source code> DESTDIR=<path to the root of the rootfs> install

**In the following replace the x with the last letter of the specific board name.**

Jailhouse is now installed on the emCON-RZ/G1x board. But we also need the configuration
of the root cell. We have to copy this configuration manually using these commands on the
command line on the development workstation:

mkdir -p /jailhouse/configs
cp configs/arm/emtrion-rzg1**x**.cell /jailhouse/configs

Now we can unmount the RootFS of the emCON-RZ/G1x board because the other steps are executed
on the emCON-RZ/G1x board. On the console of the started Linux on emCON-RZ/G1x execute the
following commands to enable Jailhouse:

modprobe jailhouse
jailhouse enable /jailhouse/configs/arm/emtrion-rzg1**x**.cell

Running the Jailhouse UART demo as an inmate on emcon-RZ/G1x
------------------------------------------------------------
The Jailhouse project contains a bare-metal inmate sample called uart-demo. This demo outputs
a string on the serial device in a loop.

First you have to copy the following files from the Jailhouse development tree into the RootFS
of the root cell:

cp configs/arm/emtrion-rzg1**x**-uart-demo.cell /jailhouse/configs
mkdir -p /jailhouse/inmates/uart-demo
cp inmates/demos/arm/uart-demo.bin /jailhouse/inmates

We assume that the config file is available in the path /jailhouse/configs and the binary file
is available in the path /jailhouse/inmates/uart-demo. Then you can start the sample using
the following commands:

jailhouse cell create /jailhouse/configs/emtrion-rzg1**x**-uart-demo.cell
jailhouse cell load emtrion-emconrzg1**x**-uart-demo /jailhouse/inmates/uart-demo/uart-demo.bin
jailhouse cell start emtrion-emconrzg1**x**-uart-demo

The uart-demo will be started as an inmate and outputs strings through the serial port SCIF4.

Running Linux as an inmate on emCON-RZ/G1x
------------------------------------------
The sample Linux inmate is setup to use the following devices:

- SCIF4 as serial console
- SDC0 where the RootFS should be stored
- I2C2

To setup the Linux inmate you have to first copy the following files from the Jailhouse tree into the RootFS
of the root cell:

cp configs/arm/emtrion-rzg1**x**-linux-demo.cell /jailhouse/configs
cp configs/arm/dts/inmate-emtrion-emconrzg1**x**.dtb /jailhouse/configs

We assume that these files are available in the path /jailhouse/configs on the RootFS of the
root cell. Make sure you have installed Python on the emCON-RZ/G1x board.

Then we can start the linux inmate executing the following command line in the root cell:

jailhouse cell linux /jailhouse/configs/emtrion-rzg1**x**-linux-demo.cell /boot/zImage -d /jailhouse/configs/inmate-emtrion-emconrzg1**x**.dtb -c "console=ttySC4,115200 cma=16M vmalloc=80M rootwait root=/dev/mmcblk0p2 vt.global_cursor_default=0 consoleblank=0"

The Linux kernel in the inmate will be started. This Linux kernel uses the devices mentioned above and
searches on the second partition of the SD card for its RootFS.
