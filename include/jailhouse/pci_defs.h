/*
 * Jailhouse, a Linux-based partitioning hypervisor
 *
 * Copyright (c) OTH Regensburg, 2019
 *
 * Authors:
 *  Ralf Ramsauer <ralf.ramsauer@oth-regensburg.de>
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

#define PCI_CAP_ID_PM			0x01 /* Power Management */
#define PCI_CAP_ID_VPD			0x03 /* Vital Product Data */
#define PCI_CAP_ID_MSI			0x05 /* Message Signalled Interrupts */
#define PCI_CAP_ID_HT			0x08 /* HyperTransport */
#define PCI_CAP_ID_VNDR			0x09 /* Vendor-Specific */
#define PCI_CAP_ID_DBG			0x0A /* Debug port */
#define PCI_CAP_ID_SSVID		0x0D /* Bridge subsystem vendor/device ID */
#define PCI_CAP_ID_SECDEV		0x0F /* Secure Device */
#define PCI_CAP_ID_EXP			0x10 /* PCI Express */
#define PCI_CAP_ID_MSIX			0x11 /* MSI-X */
#define PCI_CAP_ID_SATA			0x12 /* SATA Data/Index Conf. */
#define PCI_CAP_ID_AF			0x13 /* PCI Advanced Features */

#define PCI_EXT_CAP_ID_ERR		0x01 /* Advanced Error Reporting */
#define PCI_EXT_CAP_ID_DSN		0x03 /* Device Serial Number */
