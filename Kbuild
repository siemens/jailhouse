#
# Jailhouse, a Linux-based partitioning hypervisor
#
# Copyright (c) Siemens AG, 2013-2017
#
# Authors:
#  Jan Kiszka <jan.kiszka@siemens.com>
#  Benjamin Block <bebl@mageta.org>
#
# This work is licensed under the terms of the GNU GPL, version 2.  See
# the COPYING file in the top-level directory.
#

ALWAYS_COMPAT_MK := $(src)/scripts/always-compat.mk
export ALWAYS_COMPAT_MK

INC_CONFIG_H = $(src)/include/jailhouse/config.h
export INC_CONFIG_H

define filechk_config_mk
(									\
	echo "\$$(foreach config,\$$(filter CONFIG_%, \$$(.VARIABLES)), \
			  \$$(eval undefine \$$(config)))";		\
	if [ -f $(INC_CONFIG_H) ]; then	\
		sed -e "/^#define \([^[:space:]]*\)[[:space:]]*1/!d;	\
		        s/^#define \([^[:space:]]*\)[[:space:]]*1/\1=y/"\
		    $(INC_CONFIG_H);					\
	fi								\
)
endef

GEN_CONFIG_MK := $(obj)/hypervisor/include/generated/config.mk
export GEN_CONFIG_MK

$(GEN_CONFIG_MK): $(src)/Makefile FORCE
	$(call filechk,config_mk)

define filechk_version
	$(src)/scripts/gen_version_h $(src)/
endef

GEN_VERSION_H := $(obj)/hypervisor/include/generated/version.h

$(GEN_VERSION_H): $(src)/Makefile FORCE
	$(call filechk,version)

quiet_cmd_gen_pci_defs = GEN     $@
define cmd_gen_pci_defs
	$(filter-out FORCE,$^) > $@
endef

GEN_PCI_DEFS_PY := $(obj)/pyjailhouse/pci_defs.py

$(GEN_PCI_DEFS_PY): $(src)/scripts/gen_pci_defs.sh $(src)/include/jailhouse/pci_defs.h FORCE
	$(call if_changed,gen_pci_defs)

targets += pyjailhouse/pci_defs.py

subdir-y := hypervisor configs inmates tools

obj-m := driver/

# Do not generate files by creating dependencies if we are cleaning up
ifeq ($(filter %/Makefile.clean,$(MAKEFILE_LIST)),)

$(obj)/driver $(addprefix $(obj)/,$(subdir-y)): $(GEN_CONFIG_MK)

$(obj)/driver $(obj)/hypervisor: $(GEN_VERSION_H)

$(obj)/tools: $(GEN_PCI_DEFS_PY)

endif

clean-files := pyjailhouse/*.pyc pyjailhouse/pci_defs.py

CLEAN_DIRS := Documentation/generated hypervisor/include/generated \
	      pyjailhouse/__pycache__

ifeq ($(shell test $(VERSION) -ge 5 && test $(PATCHLEVEL) -ge 4 && echo 1),1)
clean-files += $(CLEAN_DIRS)
else
clean-dirs += $(CLEAN_DIRS)
endif
