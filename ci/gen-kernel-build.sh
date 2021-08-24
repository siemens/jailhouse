#!/bin/bash
#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2014-2021
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

BASEDIR=`cd \`dirname $0\`; pwd`

if test -z $KERNEL; then
	KERNEL=https://www.kernel.org/pub/linux/kernel/v5.x/linux-5.10.tar.xz
fi
if test -z $PARALLEL_BUILD; then
	PARALLEL_BUILD=-j16
fi
if test -z $OUTDIR; then
	OUTDIR=$BASEDIR/out
fi

prepare_out()
{
	rm -rf $OUTDIR
	mkdir -p $OUTDIR
	cd $OUTDIR
}

prepare_kernel()
{
	ARCHIVE_FILE=`basename $KERNEL`
	if ! test -f $BASEDIR/$ARCHIVE_FILE; then
		wget $KERNEL -O $BASEDIR/$ARCHIVE_FILE
	fi
	tar xJf $BASEDIR/$ARCHIVE_FILE
	ln -s linux-* linux
	cd linux
	patch -p1 << EOF
diff --git a/arch/arm/include/asm/virt.h b/arch/arm/include/asm/virt.h
index dd9697b2bde8..47600a5894b1 100644
--- a/arch/arm/include/asm/virt.h
+++ b/arch/arm/include/asm/virt.h
@@ -39,6 +39,8 @@ static inline void sync_boot_mode(void)
 	sync_cache_r(&__boot_cpu_mode);
 }
 
+void __hyp_set_vectors(unsigned long phys_vector_base);
+void __hyp_reset_vectors(void);
 #else
 #define __boot_cpu_mode	(SVC_MODE)
 #define sync_boot_mode()
@@ -73,6 +75,9 @@ static inline bool is_kernel_in_hyp_mode(void)
 
 #define HVC_SET_VECTORS 0
 #define HVC_SOFT_RESTART 1
+#define HVC_RESET_VECTORS 2
+
+#define HVC_STUB_HCALL_NR 3
 
 #endif /* __ASSEMBLY__ */
 
diff --git a/arch/arm/kernel/armksyms.c b/arch/arm/kernel/armksyms.c
index 82e96ac83684..354ab3e4e41f 100644
--- a/arch/arm/kernel/armksyms.c
+++ b/arch/arm/kernel/armksyms.c
@@ -16,6 +16,7 @@
 
 #include <asm/checksum.h>
 #include <asm/ftrace.h>
+#include <asm/virt.h>
 
 /*
  * libgcc functions - functions that are used internally by the
@@ -175,3 +176,7 @@ EXPORT_SYMBOL(__pv_offset);
 EXPORT_SYMBOL(__arm_smccc_smc);
 EXPORT_SYMBOL(__arm_smccc_hvc);
 #endif
+
+#ifdef CONFIG_ARM_VIRT_EXT
+EXPORT_SYMBOL_GPL(__boot_cpu_mode);
+#endif
diff --git a/arch/arm/kernel/hyp-stub.S b/arch/arm/kernel/hyp-stub.S
index 26d8e03b1dd3..c01622e6d9a4 100644
--- a/arch/arm/kernel/hyp-stub.S
+++ b/arch/arm/kernel/hyp-stub.S
@@ -6,6 +6,7 @@
 #include <linux/init.h>
 #include <linux/irqchip/arm-gic-v3.h>
 #include <linux/linkage.h>
+#include <asm-generic/export.h>
 #include <asm/assembler.h>
 #include <asm/virt.h>
 
@@ -189,19 +190,19 @@ ARM_BE8(orr	r7, r7, #(1 << 25))     @ HSCTLR.EE
 ENDPROC(__hyp_stub_install_secondary)
 
 __hyp_stub_do_trap:
-#ifdef ZIMAGE
 	teq	r0, #HVC_SET_VECTORS
 	bne	1f
-	/* Only the ZIMAGE stubs can change the HYP vectors */
 	mcr	p15, 4, r1, c12, c0, 0	@ set HVBAR
 	b	__hyp_stub_exit
-#endif
 
 1:	teq	r0, #HVC_SOFT_RESTART
-	bne	2f
+	bne	1f
 	bx	r1
 
-2:	ldr	r0, =HVC_STUB_ERR
+1:	teq	r0, #HVC_RESET_VECTORS
+	beq	__hyp_stub_exit
+
+	ldr	r0, =HVC_STUB_ERR
 	__ERET
 
 __hyp_stub_exit:
@@ -210,9 +211,26 @@ __hyp_stub_exit:
 ENDPROC(__hyp_stub_do_trap)
 
 /*
- * __hyp_set_vectors is only used when ZIMAGE must bounce between HYP
- * and SVC. For the kernel itself, the vectors are set once and for
- * all by the stubs.
+ * __hyp_set_vectors: Call this after boot to set the initial hypervisor
+ * vectors as part of hypervisor installation.  On an SMP system, this should
+ * be called on each CPU.
+ *
+ * r0 must be the physical address of the new vector table (which must lie in
+ * the bottom 4GB of physical address space.
+ *
+ * r0 must be 32-byte aligned.
+ *
+ * Before calling this, you must check that the stub hypervisor is installed
+ * everywhere, by waiting for any secondary CPUs to be brought up and then
+ * checking that BOOT_CPU_MODE_HAVE_HYP(__boot_cpu_mode) is true.
+ *
+ * If not, there is a pre-existing hypervisor, some CPUs failed to boot, or
+ * something else went wrong... in such cases, trying to install a new
+ * hypervisor is unlikely to work as desired.
+ *
+ * When you call into your shiny new hypervisor, sp_hyp will contain junk,
+ * so you will need to set that to something sensible at the new hypervisor's
+ * initialisation entry point.
  */
 ENTRY(__hyp_set_vectors)
 	mov	r1, r0
@@ -228,6 +246,12 @@ ENTRY(__hyp_soft_restart)
 	ret	lr
 ENDPROC(__hyp_soft_restart)
 
+ENTRY(__hyp_reset_vectors)
+	mov	r0, #HVC_RESET_VECTORS
+	__HVC(0)
+	ret	lr
+ENDPROC(__hyp_reset_vectors)
+
 #ifndef ZIMAGE
 .align 2
 .L__boot_cpu_mode_offset:
@@ -245,4 +269,5 @@ __hyp_stub_trap:	W(b)	__hyp_stub_do_trap
 __hyp_stub_irq:		W(b)	.
 __hyp_stub_fiq:		W(b)	.
 ENDPROC(__hyp_stub_vectors)
+EXPORT_SYMBOL_GPL(__hyp_stub_vectors)
 
diff --git a/arch/arm64/kernel/hyp-stub.S b/arch/arm64/kernel/hyp-stub.S
index 160f5881a0b7..a055e28be5c2 100644
--- a/arch/arm64/kernel/hyp-stub.S
+++ b/arch/arm64/kernel/hyp-stub.S
@@ -10,6 +10,7 @@
 #include <linux/linkage.h>
 #include <linux/irqchip/arm-gic-v3.h>
 
+#include <asm-generic/export.h>
 #include <asm/assembler.h>
 #include <asm/kvm_arm.h>
 #include <asm/kvm_asm.h>
@@ -42,6 +43,7 @@ SYM_CODE_START(__hyp_stub_vectors)
 	ventry	el1_fiq_invalid			// FIQ 32-bit EL1
 	ventry	el1_error_invalid		// Error 32-bit EL1
 SYM_CODE_END(__hyp_stub_vectors)
+EXPORT_SYMBOL_GPL(__hyp_stub_vectors)
 
 	.align 11
 
diff --git a/arch/x86/kernel/apic/apic.c b/arch/x86/kernel/apic/apic.c
index b3eef1d5c903..b1a6e9c550d4 100644
--- a/arch/x86/kernel/apic/apic.c
+++ b/arch/x86/kernel/apic/apic.c
@@ -196,6 +196,7 @@ static struct resource lapic_resource = {
 };
 
 unsigned int lapic_timer_period = 0;
+EXPORT_SYMBOL_GPL(lapic_timer_period);
 
 static void apic_pm_activate(void);
 
diff --git a/mm/ioremap.c b/mm/ioremap.c
index 5fa1ab41d152..d63c4ba067f9 100644
--- a/mm/ioremap.c
+++ b/mm/ioremap.c
@@ -248,6 +248,7 @@ int ioremap_page_range(unsigned long addr,
 
 	return err;
 }
+EXPORT_SYMBOL_GPL(ioremap_page_range);
 
 #ifdef CONFIG_GENERIC_IOREMAP
 void __iomem *ioremap_prot(phys_addr_t addr, size_t size, unsigned long prot)
diff --git a/mm/vmalloc.c b/mm/vmalloc.c
index 6ae491a8b210..ae80dbaf743c 100644
--- a/mm/vmalloc.c
+++ b/mm/vmalloc.c
@@ -2097,6 +2097,7 @@ struct vm_struct *__get_vm_area_caller(unsigned long size, unsigned long flags,
 	return __get_vm_area_node(size, 1, flags, start, end, NUMA_NO_NODE,
 				  GFP_KERNEL, caller);
 }
+EXPORT_SYMBOL_GPL(__get_vm_area_caller);
 
 /**
  * get_vm_area - reserve a contiguous kernel virtual area
EOF
}

build_kernel()
{
	mkdir build-$1
	cp $BASEDIR/kernel-config-$1 build-$1/.config
	make O=build-$1 olddefconfig $PARALLEL_BUILD ARCH=$2 CROSS_COMPILE=$3
	make O=build-$1 $PARALLEL_BUILD ARCH=$2 CROSS_COMPILE=$3
	# clean up some unneeded build output
	find build-$1 \( -name "*.o" -o -name "*.cmd" -o -name ".tmp_*" \) -exec rm -rf {} \;
}

package_out()
{
	cd $OUTDIR
	tar cJf kernel-build.tar.xz linux-* linux
}

prepare_out
prepare_kernel
build_kernel x86 x86_64
build_kernel banana-pi arm arm-linux-gnueabihf-
build_kernel amd-seattle arm64 aarch64-linux-gnu-
package_out
