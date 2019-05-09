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
#define PCI_EXT_CAP_ID_VC		0x02 /* Virtual Channel Capability */
#define PCI_EXT_CAP_ID_DSN		0x03 /* Device Serial Number */
#define PCI_EXT_CAP_ID_PWR		0x04 /* Power Budgeting */
#define PCI_EXT_CAP_ID_RCLD		0x05 /* Root Complex Link Declaration */
#define PCI_EXT_CAP_ID_RCILC		0x06 /* Root Complex Internal Link Control */
#define PCI_EXT_CAP_ID_RCEC		0x07 /* Root Complex Event Collector */
#define PCI_EXT_CAP_ID_MFVC		0x08 /* Multi-Function VC Capability */
#define PCI_EXT_CAP_ID_VC9		0x09 /* same as _VC */
#define PCI_EXT_CAP_ID_RCRB		0x0A /* Root Complex RB? */
#define PCI_EXT_CAP_ID_VNDR		0x0B /* Vendor-Specific */
#define PCI_EXT_CAP_ID_CAC		0x0C /* Config Access - obsolete */
#define PCI_EXT_CAP_ID_ACS		0x0D /* Access Control Services */
#define PCI_EXT_CAP_ID_ARI		0x0E /* Alternate Routing ID */
#define PCI_EXT_CAP_ID_ATS		0x0F /* Address Translation Services */
#define PCI_EXT_CAP_ID_SRIOV		0x10 /* Single Root I/O Virtualization */
#define PCI_EXT_CAP_ID_MRIOV		0x11 /* Multi Root I/O Virtualization */
#define PCI_EXT_CAP_ID_MCAST		0x12 /* Multicast */
#define PCI_EXT_CAP_ID_PRI		0x13 /* Page Request Interface */
#define PCI_EXT_CAP_ID_AMD_XXX		0x14 /* Reserved for AMD */
#define PCI_EXT_CAP_ID_REBAR		0x15 /* Resizable BAR */
#define PCI_EXT_CAP_ID_DPA		0x16 /* Dynamic Power Allocation */
#define PCI_EXT_CAP_ID_TPH		0x17 /* TPH Requester */
#define PCI_EXT_CAP_ID_LTR		0x18 /* Latency Tolerance Reporting */
#define PCI_EXT_CAP_ID_SECPCI		0x19 /* Secondary PCIe Capability */
#define PCI_EXT_CAP_ID_PMUX		0x1A /* Protocol Multiplexing */
#define PCI_EXT_CAP_ID_PASID		0x1B /* Process Address Space ID */
#define PCI_EXT_CAP_ID_DPC		0x1D /* Downstream Port Containment */
#define PCI_EXT_CAP_ID_L1SS		0x1E /* L1 PM Substates */
#define PCI_EXT_CAP_ID_PTM		0x1F /* Precision Time Measurement */
