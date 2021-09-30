#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2015, 2016
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#
# Alternatively, you can use or redistribute this file under the following
# BSD license:
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above copyright
#    notice, this list of conditions and the following disclaimer in the
#    documentation and/or other materials provided with the distribution.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
# AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
# IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
# ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
# LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
# CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
# SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
# INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
# CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
# THE POSSIBILITY OF SUCH DAMAGE.
#

include $(INMATES_LIB)/Makefile.lib

always-y := lib.a lib32.a

TARGETS := cpu-features.o excp.o header-common.o irq.o ioapic.o printk.o
TARGETS += setup.o uart.o
TARGETS += ../alloc.o ../pci.o ../string.o ../cmdline.o ../setup.o ../test.o
TARGETS += ../uart-8250.o ../printk.o
TARGETS_32_ONLY := header-32.o
TARGETS_64_ONLY := mem.o pci.o smp.o timing.o header-64.o

lib-y := $(TARGETS) $(TARGETS_64_ONLY)
lib32-y := $(TARGETS:.o=-32.o) $(TARGETS_32_ONLY)

quiet_cmd_link_archive32 = AR      $@
cmd_link_archive32 = rm -f $@; $(AR) rcs$(KBUILD_ARFLAGS) $@ $(filter-out FORCE,$^)

$(obj)/lib32.a: $(addprefix $(obj)/,$(lib32-y)) FORCE
	$(call if_changed,link_archive32)

targets += $(lib32-y)

# Code of this object is called before SSE/AVX extensions are available. Ensure
# that the compiler won't generate unsupported instructions for this file.
CFLAGS_cpu-features.o += -mno-sse

$(obj)/%-32.o: c_flags += -m32
$(obj)/%-32.o: $(src)/%.c FORCE
	$(call if_changed_rule,cc_o_c)

$(obj)/%-32.o: a_flags += -m32
$(obj)/%-32.o: $(src)/%.S FORCE
	$(call if_changed_rule,as_o_S)
