/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2013
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <asm/types.h>

struct acpi_table_header {
	u32 signature;
	u32 length;
	u8 revision;
	u8 checksum;
	char oem_id[6];
	char oem_table_id[8];
	u32 oem_revision;
	char asl_compiler_id[4];
	u32 asl_compiler_revision;
};

const struct acpi_table_header *
acpi_find_table(char name[4], const struct acpi_table_header *start);
