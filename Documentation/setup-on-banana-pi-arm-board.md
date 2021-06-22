Setup on Banana Pi ARM board
============================

The Banana Pi is a cheap Raspberry-Pi-like ARM board with an Allwinner A20 SoC
(dual-core Cortex-A7). It runs mainline Linux kernels and U-Boot and is
comparably well hackable. Further information can be found on
http://linux-sunxi.org.

In order to run Jailhouse, the Linux kernel version should be at least 3.19. The
configuration used for continuous integration builds can serve as reference, see
`ci/kernel-config-banana-pi`. The kernel requires a small patch in order to
build the Jailhouse driver module:

```diff
diff --git a/arch/arm/kernel/armksyms.c b/arch/arm/kernel/armksyms.c
index 7e45f69..dc0336e 100644
--- a/arch/arm/kernel/armksyms.c
+++ b/arch/arm/kernel/armksyms.c
@@ -20,6 +20,7 @@
 
 #include <asm/checksum.h>
 #include <asm/ftrace.h>
+#include <asm/virt.h>
 
 /*
  * libgcc functions - functions that are used internally by the
@@ -181,3 +182,7 @@ EXPORT_SYMBOL(__pv_offset);
 EXPORT_SYMBOL(arm_smccc_smc);
 EXPORT_SYMBOL(arm_smccc_hvc);
 #endif
+
+#ifdef CONFIG_ARM_VIRT_EXT
+EXPORT_SYMBOL_GPL(__boot_cpu_mode);
+#endif
```

Meanwhile, an U-Boot release more recent than v2015.04 is required. Tested and
known to work is release v2016.03. Note that, **since v2015.10, you need to
disable CONFIG_VIDEO in the U-Boot config**, or U-Boot will configure the
framebuffer at the end of the physical RAM where Jailhouse is located.

Below is a tutorial about setting up Jailhouse on a **BananaPi M1** board, and
running [FreeRTOS-cell](https://github.com/siemens/freertos-cell) on the top of
it. The tutorial is based on:
 - Ubuntu-14.04 on a x86-64 machine
 - BananaPi M1 board, running bananian-16.04


Installation
------------
Follow the instructions on [BananaPi official site](https://www.bananian.org/download)
to build your sd-card.

Basically here are the steps,
```bash
#On Ubuntu-14.04 (or any other machine),
#HERE WE USE **BANANIAN** AS OUR BANANAPI'S OS
$ wget https://dl.bananian.org/releases/bananian-latest.zip
$ sudo apt-get update && sudo apt-get install unzip screen
$ unzip ./bananian-latest.zip
$ lsblk | grep mmcblk

# Write the image to sdcard, replace `mmcblk0` below with the device name
# returned from `lsblk`
$ sudo dd bs=1M if=~/bananian-*.img of=/dev/mmcblk0

#Insert the sd-card to BananaPi, connect it to our machine using ttl cable.
#On Ubuntu,
$ screen /dev/ttyUSB0 115200

#On BananaPi, login with root/pi, then expand the filesystem
$ bananian-config

#Choose `y` for `Do you want to expand the root file system`.
#Feel free to configure other stuff as well.
```


Adjusting U-boot for kernel booting arguments.
---------------------------------------------
Jailhouse needs to boot with certain Kernel arguments to reserve memory for
other cells (using `mem=...`). We must adjust U-boot config file to boot with
these arguments. The u-boot partition is not mounted by default. Thus, we need
to mount it first. On bananian,
```bash
$ mkdir /p1
$ mount /dev/mmcblk0p1 /p1
$ vi /p1/boot.cmd
```
Append `mem=932M vmalloc=512M` on the end of line that starts with
`setenv bootargs`. After editing, the file should look like:
```bash
#-------------------------------------------------------------------------------
# Boot loader script to boot with different boot methods for old and new kernel
# Credits: https://github.com/igorpecovnik - Thank you for this great script!
#-------------------------------------------------------------------------------
if load mmc 0:1 0x00000000 uImage-next
then
# mainline kernel >= 4.x
#-------------------------------------------------------------------------------
setenv bootargs console=ttyS0,115200 console=tty0 console=tty1 root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline rootwait mem=932M vmalloc=512M
load mmc 0:1 0x49000000 dtb/${fdtfile}
load mmc 0:1 0x46000000 uImage-next
bootm 0x46000000 - 0x49000000
#-------------------------------------------------------------------------------
else
# sunxi 3.4.x
#-------------------------------------------------------------------------------
setenv bootargs console=ttyS0,115200 console=tty0 console=tty1 sunxi_g2d_mem_reserve=0 sunxi_ve_mem_reserve=0 hdmi.audio=EDID:0 disp.screen0_output_mode=EDID:1680x1050p60 root=/dev/mmcblk0p2 rootfstype=ext4 elevator=deadline rootwait
setenv bootm_boot_mode sec
load mmc 0:1 0x43000000 script.bin
load mmc 0:1 0x48000000 uImage
bootm 0x48000000
#-------------------------------------------------------------------------------
fi
```
After saving the file, create u-boot recognizable image `*.src` from `*.cmd`
using mkimage
```bash
$ apt-get update && apt-get install -y u-boot-tools
$ cd /p1
$ mkimage -C none -A arm -T script -d boot.cmd boot.scr
```
See more on [disccusion](https://groups.google.com/forum/#!topic/jailhouse-dev/LzyOqEHvEk0)
about why it's `mem=932M` instead of `mem=958M`.


Cross Compiling Kernel for ARM on x86
-------------------------------------
Jailhouse need to be compiled with kernel objects. Thus we first need a copy of
kernel source code and compile it.

Jailhouse requires Kernel Version >= 3.19. Here, we'll use Kernel version 4.3.3
with patch provided by Bananian team.

We'll cross compile on a x86 machine for faster compilation. On the compiling
machine,
* Obtaining Kernel source code & patches for bananapi, and jailhouse
```bash
$ sudo apt-get update && sudo apt-get install -y git
$ cd ~
$ git clone git://git.kernel.org/pub/scm/linux/kernel/git/stable/linux-stable.git
$ git clone https://github.com/Bananian/bananian.git
$ git clone https://github.com/siemens/jailhouse.git
```

* Obtaining cross compiling tool-chain from [Linaro Official site](http://www.linaro.org/downloads/)
```bash
#Download the recommended cross-toolchain from Linaro

$ cd ~
$ wget https://releases.linaro.org/components/toolchain/binaries/latest-5/arm-linux-gnueabihf/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf.tar.xz
$ tar -xf gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf.tar.xz

#Update environment path
$ export PATH=$PATH:$(pwd)/gcc-linaro-5.3-2016.02-x86_64_arm-linux-gnueabihf/bin
```

* Apply patches and Setup config
```bash
$ cd ~/linux-stable
$ git checkout v4.3.3
$ for i in ../bananian/kernel/4.3.3/patches/*; do patch -p1 < $i; done

#Copy config from jailhouse/ci directory
$ cp -av ../jailhouse/ci/kernel-config-banana-pi .config

$ sudo apt-get update && sudo apt-get install -y build-essential libncurses5 libncurses5-dev
$ make ARCH=arm menuconfig
#Enable FUSE under "File systems", needed for file transfer via sshfs
```

* Compile Kernel
```bash
$ sudo apt-get update && sudo apt-get install -y u-boot-tools
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- -j8 uImage modules dtbs LOADADDR=40008000
```


Installing Kernel
-----------------
Here we choose to mount compiled kernel-source directory on BananaPi. Instead
one can choose to transfer the directory that contains kernel source to
BananaPi.
```bash
#On Compiling machine,
$ sudo apt-get update && sudo apt-get install -y sshfs

#On BananaPi,
$ apt-get update && apt-get install -y sshfs make gcc
$ mkdir ~/linux-src
$ sshfs <remote user>@<remote ip address>:<linux source path> ~/linux-src
$ cd ~/linux-src
$ make modules_install

#Update U-boot partition
$ mount /dev/mmcblk0p1 /boot
$ mkdir /boot/dtb/
$ cp -v arch/arm/boot/uImage /boot/uImage-next
$ cp -v arch/arm/boot/dts/*.dtb /boot/dtb/

#now reboot
$ reboot
```

Verify installation using `$ uname -r`, if the kernel is installed successfully,
the bash command above should return `4.3.3-dirty`.


Cross Compiling Jailhouse(w/ FreeRTOS-cell) for ARM on x86
-----------------------------------------------------
* Check the need of upgrading `make`.
Jailhouse require make>=3.82 to build it, we might need to upgrade `make`.
```bash
#On Compiling machine,
$ make --version

#If make>=3.82, skip this part and continue with "Building Jailhouse"
$ sudo apt-get update && sudo apt-get install -y checkinstall
$ wget http://ftp.gnu.org/gnu/make/make-3.82.tar.bz2 -O ~/Downloads/make-3.82.tar.bz2
$ cd ~ && tar -xf ~/Downloads/make-3.82.tar.bz2
$ cd make-3.82
$ ./configure --prefix=/usr
$ make
$ sudo checkinstall make install
```

* Building Jailhouse (mainly for FreeRTOR as a cell)
```bash
#On Compiling Machine,
$ sudo apt-get update && sudo apt-get install -y python3-mako device-tree-compiler
$ cd ~
$ git clone https://github.com/siemens/freertos-cell
$ cp -av ~/freertos-cell/jailhouse-configs/bananapi.c ~/jailhouse/configs/arm/bananapi.c
$ cp -av freertos-cell/jailhouse-configs/bananapi-freertos-demo.c ~/jailhouse/configs/arm/

#Copy the configuration header file before building
$ cp -av ~/jailhouse/ci/jailhouse-config-banana-pi.h ~/jailhouse/include/jailhouse/config.h
$ cd ~/jailhouse
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=../linux-stable
```

* Build FreeRTOS-cell
```bash
$ cd ~/freertos-cell
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- KDIR=../linux-stable
```


Installing Jailhouse
--------------------
Here we mount `/` of BananaPi on `~/bpi_root` on our compiling Machine,
```bash
#On Compiling Machine,
$ mkdir ~/bpi_root
$ sshfs root@<bananapi_ip_addr>:/ ~/bpi_root
$ cd ~/jailhouse
$ make ARCH=arm CROSS_COMPILE=arm-linux-gnueabihf- \
     KDIR=../linux-stable DESTDIR=~/bpi_root install
```


Testing Jailhouse On BananaPi
-----------------------------
To test jailhouse on BananaPi, we would need the `*.cell` file where could be
located in the cross-compiled `jailhouse` & `freertos-cell` directory.

Here we transfer these two directory from the compiling machine to BananaPi
using sftp.
```bash
#On Compiling Machine,
$ cd ~ && tar -zcf jailhouse-compiled.tar.gz jailhouse freertos-cell
$ sftp root@<bananapi_ip_addr>
sftp > put jailhouse-compiled.tar.gz
sftp > quit

#On BananaPi,
$ cd ~ && tar -xf jailhouse-compiled.tar.gz
$ modprobe jailhouse

#If nothing goes wrong, you should see jailhouse using a `$ lsmod`
$ jailhouse enable ~/jailhouse/configs/arm/bananapi.cell
$ jailhouse cell create ~/jailhouse/configs/arm/bannapi-freertos-demo.cell
$ jailhouse cell load FreeRTOS ~/freertos-cell/freertos-demo.bin
$ jailhouse cell start FreeRTOS

#Jailhouse and FreeRTOS cell has been started, you should able to get some
#output from the FreeRTOS demo application on the second serial interface of
#BananaPi.

#After making sure all things works well, turn off jailhouse using command below
$ jailhouse cell shutdown FreeRTOS
$ jailhouse cell destroy FreeRTOS
$ jailhouse disable
$ rmmod jailhouse
```


References
----------
1. https://github.com/cyng93/jailhouse/blob/master/README.md
2. https://github.com/siemens/freertos-cell/blob/master/README.md
3. https://groups.google.com/forum/#!topic/jailhouse-dev/LzyOqEHvEk0
4. https://fossapc.hackpad.com/Jailhouse-on-Banana-Pi-with-FreeRTOS-as-Cell-EZaCw8OEynM
5. https://embedded2015.hackpad.com/ep/pad/static/P8aM30iQ3hm
6. https://paper.dropbox.com/doc/Banana-Pi-and-jailhouse-6CoZ4Cgyom22Gy3uH9g7e
