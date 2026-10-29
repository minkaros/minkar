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
 * NVM Express definitions used by MinkarNVMe, written from the public NVM
 * Express Base Specification (nvmexpress.org): controller registers, the
 * submission and completion queue entries, and the admin and NVM command
 * set opcodes the driver uses.
 */

#ifndef _MINKAR_NVME_H
#define _MINKAR_NVME_H

#include <stdint.h>

/* Controller registers (BAR0). */
#define NVME_REG_CAP		0x00	/* 64-bit */
#define NVME_REG_VS		0x08
#define NVME_REG_INTMS		0x0C
#define NVME_REG_INTMC		0x10
#define NVME_REG_CC		0x14
#define NVME_REG_CSTS		0x1C
#define NVME_REG_AQA		0x24
#define NVME_REG_ASQ		0x28	/* 64-bit */
#define NVME_REG_ACQ		0x30	/* 64-bit */
#define NVME_REG_DOORBELL	0x1000

/* CAP fields. */
#define NVME_CAP_MQES(c)	((uint32_t)((c) & 0xFFFF))		/* 0-based */
#define NVME_CAP_TO(c)		((uint32_t)(((c) >> 24) & 0xFF))	/* 500 ms units */
#define NVME_CAP_DSTRD(c)	((uint32_t)(((c) >> 32) & 0xF))
#define NVME_CAP_CSS_NVM(c)	(((c) >> 37) & 1)
#define NVME_CAP_MPSMIN(c)	((uint32_t)(((c) >> 48) & 0xF))
#define NVME_CAP_MPSMAX(c)	((uint32_t)(((c) >> 52) & 0xF))

/* CC fields. */
#define NVME_CC_EN		(1U << 0)
#define NVME_CC_CSS_NVM		(0U << 4)
#define NVME_CC_MPS(n)		((uint32_t)(n) << 7)	/* page = 2^(12+n) */
#define NVME_CC_AMS_RR		(0U << 11)
#define NVME_CC_SHN_MASK	(3U << 14)
#define NVME_CC_SHN_NORMAL	(1U << 14)
#define NVME_CC_IOSQES(n)	((uint32_t)(n) << 16)
#define NVME_CC_IOCQES(n)	((uint32_t)(n) << 20)

/* CSTS fields. */
#define NVME_CSTS_RDY		(1U << 0)
#define NVME_CSTS_CFS		(1U << 1)
#define NVME_CSTS_SHST_MASK	(3U << 2)
#define NVME_CSTS_SHST_DONE	(2U << 2)

/* Submission queue entry: 64 bytes. */
struct nvme_sqe {
	uint8_t		opc;
	uint8_t		flags;		/* fused, PSDT (0: PRPs) */
	uint16_t	cid;
	uint32_t	nsid;
	uint32_t	cdw2;
	uint32_t	cdw3;
	uint64_t	mptr;
	uint64_t	prp1;
	uint64_t	prp2;
	uint32_t	cdw10;
	uint32_t	cdw11;
	uint32_t	cdw12;
	uint32_t	cdw13;
	uint32_t	cdw14;
	uint32_t	cdw15;
};

/* Completion queue entry: 16 bytes. */
struct nvme_cqe {
	uint32_t	dw0;
	uint32_t	dw1;
	uint16_t	sqhd;
	uint16_t	sqid;
	uint16_t	cid;
	uint16_t	status;		/* bit 0: phase tag; 15:1 status field */
};

#define NVME_CQE_PHASE(s)	((s) & 1)
#define NVME_CQE_SC(s)		(((s) >> 1) & 0xFF)	/* status code */
#define NVME_CQE_SCT(s)		(((s) >> 9) & 0x7)	/* status code type */

/* Admin commands. */
#define NVME_ADM_DELETE_SQ	0x00
#define NVME_ADM_CREATE_SQ	0x01
#define NVME_ADM_DELETE_CQ	0x04
#define NVME_ADM_CREATE_CQ	0x05
#define NVME_ADM_IDENTIFY	0x06
#define NVME_ADM_SET_FEATURES	0x09

#define NVME_CNS_NAMESPACE	0x00
#define NVME_CNS_CONTROLLER	0x01

#define NVME_FEAT_NUM_QUEUES	0x07

/* Create I/O queue flags (CDW11). */
#define NVME_QUEUE_PC		(1U << 0)	/* physically contiguous */
#define NVME_CQ_IEN		(1U << 1)	/* interrupts enabled */

/* NVM command set. */
#define NVME_CMD_FLUSH		0x00
#define NVME_CMD_WRITE		0x01
#define NVME_CMD_READ		0x02

/* Identify Controller (4096 bytes): the offsets the driver reads. */
#define NVME_IDC_SN		4	/* 20 bytes, ASCII */
#define NVME_IDC_MN		24	/* 40 bytes, ASCII */
#define NVME_IDC_FR		64	/* 8 bytes, ASCII */
#define NVME_IDC_MDTS		77
#define NVME_IDC_NN		516	/* 32-bit */
#define NVME_IDC_VWC		525

/* Identify Namespace (4096 bytes). */
#define NVME_IDN_NSZE		0	/* 64-bit, in logical blocks */
#define NVME_IDN_NLBAF		25
#define NVME_IDN_FLBAS		26
#define NVME_IDN_LBAF		128	/* 16 formats of 4 bytes */
#define NVME_LBAF_LBADS(f)	(((f) >> 16) & 0xFF)

#endif /* !_MINKAR_NVME_H */
