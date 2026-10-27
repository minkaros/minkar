/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Constants and layouts of the Virtual I/O Device (VIRTIO) specification,
 * version 1.x (OASIS): the PCI transport, split virtqueues, and the block
 * and network devices. Written from the public specification; all fields
 * are little-endian, as is x86.
 */

#ifndef _VIRTIOSPEC_H
#define _VIRTIOSPEC_H

#include <stdint.h>

/* PCI identity: vendor 0x1AF4; transitional device IDs 0x1000 + type - 1
 * for the first types, modern ones 0x1040 + type. */
#define VIRTIO_PCI_VENDOR		0x1AF4
#define VIRTIO_ID_NET			1
#define VIRTIO_ID_BLOCK			2

/* Vendor-specific PCI capabilities that locate the configuration areas. */
#define VIRTIO_PCI_CAP_COMMON_CFG	1
#define VIRTIO_PCI_CAP_NOTIFY_CFG	2
#define VIRTIO_PCI_CAP_ISR_CFG		3
#define VIRTIO_PCI_CAP_DEVICE_CFG	4
#define VIRTIO_PCI_CAP_PCI_CFG		5

/* Offsets within a virtio PCI capability. */
#define VIRTIO_PCI_CAP_CFG_TYPE		3	/* u8 */
#define VIRTIO_PCI_CAP_BAR		4	/* u8 */
#define VIRTIO_PCI_CAP_OFFSET		8	/* le32 */
#define VIRTIO_PCI_CAP_LENGTH		12	/* le32 */
#define VIRTIO_PCI_NOTIFY_MULTIPLIER	16	/* le32, notify capability only */

/* The common configuration structure. */
#define VIRTIO_COMMON_DFSELECT		0	/* le32 device_feature_select */
#define VIRTIO_COMMON_DF		4	/* le32 device_feature */
#define VIRTIO_COMMON_GFSELECT		8	/* le32 driver_feature_select */
#define VIRTIO_COMMON_GF		12	/* le32 driver_feature */
#define VIRTIO_COMMON_MSIX		16	/* le16 config_msix_vector */
#define VIRTIO_COMMON_NUMQ		18	/* le16 num_queues */
#define VIRTIO_COMMON_STATUS		20	/* u8 device_status */
#define VIRTIO_COMMON_CFGGEN		21	/* u8 config_generation */
#define VIRTIO_COMMON_Q_SELECT		22	/* le16 */
#define VIRTIO_COMMON_Q_SIZE		24	/* le16 */
#define VIRTIO_COMMON_Q_MSIX		26	/* le16 */
#define VIRTIO_COMMON_Q_ENABLE		28	/* le16 */
#define VIRTIO_COMMON_Q_NOFF		30	/* le16 queue_notify_off */
#define VIRTIO_COMMON_Q_DESC		32	/* le64 */
#define VIRTIO_COMMON_Q_AVAIL		40	/* le64 (driver area) */
#define VIRTIO_COMMON_Q_USED		48	/* le64 (device area) */

#define VIRTIO_MSI_NO_VECTOR		0xFFFF

/* Device status. */
#define VIRTIO_STATUS_ACKNOWLEDGE	0x01
#define VIRTIO_STATUS_DRIVER		0x02
#define VIRTIO_STATUS_DRIVER_OK		0x04
#define VIRTIO_STATUS_FEATURES_OK	0x08
#define VIRTIO_STATUS_NEEDS_RESET	0x40
#define VIRTIO_STATUS_FAILED		0x80

/* ISR status bits (reading the register clears it). */
#define VIRTIO_ISR_QUEUE		0x01
#define VIRTIO_ISR_CONFIG		0x02

/* Feature bits common to all devices. */
#define VIRTIO_F_VERSION_1		32

/* Split virtqueue layout. */
#define VIRTQ_DESC_F_NEXT		1
#define VIRTQ_DESC_F_WRITE		2

struct virtq_desc {
	uint64_t	addr;
	uint32_t	len;
	uint16_t	flags;
	uint16_t	next;
} __attribute__((packed));

struct virtq_used_elem {
	uint32_t	id;
	uint32_t	len;
} __attribute__((packed));

/* Block device. */
#define VIRTIO_BLK_F_SIZE_MAX		1
#define VIRTIO_BLK_F_SEG_MAX		2
#define VIRTIO_BLK_F_RO			5
#define VIRTIO_BLK_F_BLK_SIZE		6
#define VIRTIO_BLK_F_FLUSH		9

#define VIRTIO_BLK_CFG_CAPACITY		0	/* le64, in 512-byte sectors */
#define VIRTIO_BLK_CFG_SIZE_MAX		8	/* le32 */
#define VIRTIO_BLK_CFG_SEG_MAX		12	/* le32 */
#define VIRTIO_BLK_CFG_BLK_SIZE		20	/* le32 */

#define VIRTIO_BLK_T_IN			0
#define VIRTIO_BLK_T_OUT		1
#define VIRTIO_BLK_T_FLUSH		4
#define VIRTIO_BLK_T_GET_ID		8

#define VIRTIO_BLK_S_OK			0
#define VIRTIO_BLK_S_IOERR		1
#define VIRTIO_BLK_S_UNSUPP		2

#define VIRTIO_BLK_ID_BYTES		20

struct virtio_blk_req_hdr {
	uint32_t	type;
	uint32_t	reserved;
	uint64_t	sector;
} __attribute__((packed));

/* Network device. */
#define VIRTIO_NET_F_MAC		5
#define VIRTIO_NET_F_STATUS		16

#define VIRTIO_NET_CFG_MAC		0	/* u8[6] */
#define VIRTIO_NET_CFG_STATUS		6	/* le16 */
#define VIRTIO_NET_S_LINK_UP		1

#define VIRTIO_NET_RX_QUEUE		0
#define VIRTIO_NET_TX_QUEUE		1

/* The header in front of every packet (VERSION_1 always has num_buffers). */
struct virtio_net_hdr {
	uint8_t		flags;
	uint8_t		gso_type;
	uint16_t	hdr_len;
	uint16_t	gso_size;
	uint16_t	csum_start;
	uint16_t	csum_offset;
	uint16_t	num_buffers;
} __attribute__((packed));

#endif /* !_VIRTIOSPEC_H */
