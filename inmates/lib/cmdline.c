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
 *
 * Alternatively, you can use or redistribute this file under the following
 * BSD license:
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <inmate.h>

#define CMDLINE_BUFFER_SIZE 256
CMDLINE_BUFFER(CMDLINE_BUFFER_SIZE) __attribute__((weak));

static bool get_param(const char *param, char *value_buffer,
		      unsigned long buffer_size)
{
	unsigned long param_len = strlen(param);
	const char *p = cmdline;

	while (1) {
		/* read over leading blanks */
		while (*p == ' ')
			p++;

		if (strncmp(p, param, param_len) == 0) {
			p += param_len;

			*value_buffer = 0;
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
			}
			return true;
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

bool cmdline_parse_bool(const char *param, bool default_value)
{
	char value_buffer[8];

	/* return the default value if the parameter is not provided */
	if (!get_param(param, value_buffer, sizeof(value_buffer)))
		return default_value;

	if (!strncasecmp(value_buffer, "true", 4) ||
	    !strncmp(value_buffer, "1", 1) || strlen(value_buffer) == 0)
		return true;

	if (!strncasecmp(value_buffer, "false", 5) ||
	    !strncmp(value_buffer, "0", 1))
		return false;

	return default_value;
}
