/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) Siemens AG, 2016
 *
 * Authors:
 *  Jan Kiszka <jan.kiszka@siemens.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#include <inmate.h>

extern const char cmdline[];

static bool get_param(const char *param, char *value_buffer,
		      unsigned long buffer_size)
{
	unsigned long param_len = strlen(param);
	const char *p = cmdline;

	while (1) {
		/* read over leading blanks */
		while (*p == ' ') {
			if (*p == 0)
				return false;
			p++;
		}

		if (strncmp(p, param, param_len) == 0) {
			p += param_len;

			/* check for boolean parameter */
			if ((buffer_size == 0 && (*p == ' ' || *p == 0)))
				return true;

			/* extract parameter value */
			if (*p == '=') {
				p++;
				while (buffer_size > 1) {
					if (*p == ' ' || *p == 0)
						break;
					*value_buffer++ = *p++;
					buffer_size--;
				}
				if (buffer_size > 0)
					*value_buffer = 0;
				return true;
			}
		}

		/* search for end of this parameter */
		while (*p != ' ') {
			if (*p == 0)
				return false;
			p++;
		}
	}
}

const char *cmdline_parse_str(const char *param, char *value_buffer,
			      unsigned long buffer_size,
			      const char *default_value)
{
	if (get_param(param, value_buffer, buffer_size))
		return value_buffer;
	else
		return default_value;
}

long long cmdline_parse_int(const char *param, long long default_value)
{
	char value_buffer[32];
	char *p = value_buffer;
	bool negative = false;
	long long value = 0;

	if (!get_param(param, value_buffer, sizeof(value_buffer)))
		return default_value;

	if (strncmp(p, "0x", 2) == 0) {
		p += 2;
		do {
			if (*p >= '0' && *p <= '9')
				value = (value << 4) + *p - '0';
			else if (*p >= 'A' && *p <= 'F')
				value = (value << 4) + *p - 'A' + 10;
			else if (*p >= 'a' && *p <= 'f')
				value = (value << 4) + *p - 'a' + 10;
			else
				return default_value;
			p++;
		} while (*p != 0);
	} else {
		if (*p == '-' || *p == '+')
			negative = (*p++ == '-');

		do {
			if (*p >= '0' && *p <= '9')
				value = (value * 10) + *p - '0';
			else
				return default_value;
			p++;
		} while (*p != 0);
	}

	return negative ? -value : value;
}

bool cmdline_parse_bool(const char *param)
{
	return get_param(param, NULL, 0);
}
