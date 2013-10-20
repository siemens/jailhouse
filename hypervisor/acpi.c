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

#include <jailhouse/acpi.h>
#include <jailhouse/control.h>
#include <jailhouse/entry.h>

static bool acpi_valid_checksum(const struct acpi_table_header *table)
{
	const u8 *pos = (const u8 *)table;
	const u8 *end = pos + table->length;
	u8 sum = 0;

	while (pos < end)
		sum += *pos++;
	return sum == 0;
}

const struct acpi_table_header *
acpi_find_table(char name[4], const struct acpi_table_header *start)
{
	void *end = config_memory + system_config->config_memory.size;
	const struct acpi_table_header *tab;
	const void *pos;

	pos = start ? ((const void *)start) + start->length : config_memory;
	while ((pos + sizeof(struct acpi_table_header)) < end) {
		tab = pos++;

		if (tab->signature == *(u32 *)name &&
		    (pos + tab->length) < end && acpi_valid_checksum(tab))
			return tab;
	}

	return NULL;
}
