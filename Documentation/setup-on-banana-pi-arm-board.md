Setup on Banana Pi ARM board
----------------------------

The Banana Pi is a cheap Raspberry-Pi-like ARM board with an Allwinner A20 SoC
(dual-core Cortex-A7). It runs mainline Linux kernels and U-Boot and is
comparably well hackable. Further information can be found on
http://linux-sunxi.org.

For Jailhouse, an U-Boot release more recent than v2015.04 is required. Tested
and known to work is release v2016.03. Note that, since v2015.10, you need to
disable CONFIG_VIDEO in the U-Boot config, or U-Boot will configure the
framebuffer at the end of the physical RAM where Jailhouse is located.

The Linux kernel version should be at least 3.19. The configuration used for
continuous integration builds can serve as reference, see
`ci/kernel-config-banana-pi`. The kernel has to be booted with the following
additional parameters, e.g. by adjusting the U-Boot environment accordingly:

    mem=958M vmalloc=512M

The recommended cross-toolchain is available from Linaro, see
http://www.linaro.org/downloads.

Before building Jailhouse, copy the configuration header file
`ci/jailhouse-config-banana-pi.h` to `hypervisor/include/jailhouse/config.h`.
Then run make:

    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
         KDIR=/path/to/arm-kernel/objects

Binaries can be installed directly to the target root file system if it is
mounted on the host:

    make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
         KDIR=/path/to/arm-kernel/objects DESTDIR=/mount-point install

Cell configurations and demo inmates will not be installed this way and have to
be transferred manually as needed. Make sure you have `configs/bananapi.cell`
and, as desired, the inmates configs (`configs/bananapi-*.cell`) and binaries
(`inmates/demos/arm/*.bin`) available on the target.

Jailhouse and inmates are started on ARM just like on x86. The only difference
is that inmates have to be loaded at offset 0. Just leave out the `-a`
parameter when invoking `jailhouse cell load`.

