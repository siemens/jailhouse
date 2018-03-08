/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Authors:
 *  Peng Fang <peng.fan@nxp.com>
 *  Claudio Scordino <claudio@evidence.eu.com>
 *
 * This work is licensed under the terms of the GNU GPL, version 2.  See
 * the COPYING file in the top-level directory.
 */

#define IS_SIP_32(hvc)			(((hvc) >> 24) == 0x82)
#define IS_SIP_64(hvc)			(((hvc) >> 24) == 0xc2)

#define SIP_NOT_SUPPORTED		(-1)
