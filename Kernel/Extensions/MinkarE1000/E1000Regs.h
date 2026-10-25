/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
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
 * Registers and descriptors of the Intel 8254x (82540EM, 82545EM, ...) and
 * 82574L Gigabit Ethernet controllers used by MinkarE1000, from Intel's
 * public software developer's manuals. Only the legacy descriptor formats
 * are used, which every member of the family supports.
 */

#ifndef _E1000REGS_H
#define _E1000REGS_H

#include <stdint.h>

/* Register offsets (BAR 0, memory mapped). */
#define E1000_CTRL	0x00000	/* device control */
#define E1000_STATUS	0x00008	/* device status */
#define E1000_EECD	0x00010	/* EEPROM control */
#define E1000_EERD	0x00014	/* EEPROM read */
#define E1000_CTRL_EXT	0x00018	/* extended device control */
#define E1000_MDIC	0x00020	/* MDI control: PHY registers */
#define E1000_ICR	0x000C0	/* interrupt cause read (clears on read) */
#define E1000_ITR	0x000C4	/* interrupt throttling */
#define E1000_ICS	0x000C8	/* interrupt cause set */
#define E1000_IMS	0x000D0	/* interrupt mask set */
#define E1000_IMC	0x000D8	/* interrupt mask clear */
#define E1000_RCTL	0x00100	/* receive control */
#define E1000_TCTL	0x00400	/* transmit control */
#define E1000_TIPG	0x00410	/* transmit inter-packet gap */
#define E1000_RDBAL	0x02800	/* receive descriptor base, low */
#define E1000_RDBAH	0x02804	/* receive descriptor base, high */
#define E1000_RDLEN	0x02808	/* receive ring length in bytes */
#define E1000_RDH	0x02810	/* receive head */
#define E1000_RDT	0x02818	/* receive tail */
#define E1000_RDTR	0x02820	/* receive delay timer */
#define E1000_TDBAL	0x03800	/* transmit descriptor base, low */
#define E1000_TDBAH	0x03804	/* transmit descriptor base, high */
#define E1000_TDLEN	0x03808	/* transmit ring length in bytes */
#define E1000_TDH	0x03810	/* transmit head */
#define E1000_TDT	0x03818	/* transmit tail */
#define E1000_TIDV	0x03820	/* transmit interrupt delay */
#define E1000_MTA	0x05200	/* multicast table, 128 words */
#define E1000_RXCSUM	0x05000	/* receive checksum control */
#define E1000_VET	0x00038	/* VLAN ether type */

/* Statistics registers (clear on read). */
#define E1000_CRCERRS	0x04000	/* CRC errors */
#define E1000_ALGNERRC	0x04004	/* alignment errors */
#define E1000_RXERRC	0x0400C	/* receive errors */
#define E1000_MPC	0x04010	/* missed packets */
#define E1000_SCC	0x04014	/* single collisions */
#define E1000_ECOL	0x04018	/* excessive collisions */
#define E1000_MCC	0x0401C	/* multiple collisions */
#define E1000_LATECOL	0x04020	/* late collisions */
#define E1000_COLC	0x04028	/* collisions */
#define E1000_DC	0x04030	/* deferred */
#define E1000_TNCRS	0x04034	/* transmit with no carrier sense */
#define E1000_RNBC	0x040A0	/* receive no buffers */
#define E1000_RUC	0x040A4	/* receive undersize */
#define E1000_ROC	0x040AC	/* receive oversize */

/* RXCSUM bits. */
#define E1000_RXCSUM_IPOFL	(1U << 8)	/* IP checksum offload */
#define E1000_RXCSUM_TUOFL	(1U << 9)	/* TCP/UDP checksum offload */
#define E1000_RAL0	0x05400	/* receive address 0, low */
#define E1000_RAH0	0x05404	/* receive address 0, high */

/* CTRL bits. */
#define E1000_CTRL_FD		(1U << 0)	/* full duplex */
#define E1000_CTRL_LRST		(1U << 3)	/* link reset */
#define E1000_CTRL_ASDE		(1U << 5)	/* auto speed detection */
#define E1000_CTRL_SLU		(1U << 6)	/* set link up */
#define E1000_CTRL_RST		(1U << 26)	/* device reset */
#define E1000_CTRL_VME		(1U << 30)	/* VLAN mode */
#define E1000_CTRL_PHY_RST	(1U << 31)	/* PHY reset */

/* MDIC: a PHY register read or write. */
#define E1000_MDIC_REG_SHIFT	16
#define E1000_MDIC_PHY_SHIFT	21
#define E1000_MDIC_OP_WRITE	(1U << 26)
#define E1000_MDIC_OP_READ	(2U << 26)
#define E1000_MDIC_READY	(1U << 28)
#define E1000_MDIC_ERROR	(1U << 30)
#define E1000_PHY_ADDR		1	/* the internal PHY */

/* PHY registers (IEEE 802.3 clause 22) and the bits used. */
#define MII_BMCR		0	/* control */
#define MII_BMCR_RESTART_AN	(1U << 9)
#define MII_BMCR_AUTONEG	(1U << 12)
#define MII_ANAR		4	/* autonegotiation advertisement */
#define MII_ANAR_CSMA		0x0001
#define MII_ANAR_10_HD		(1U << 5)
#define MII_ANAR_10_FD		(1U << 6)
#define MII_ANAR_100_HD		(1U << 7)
#define MII_ANAR_100_FD		(1U << 8)
#define MII_ANAR_PAUSE		(3U << 10)	/* symmetric and asymmetric */
#define MII_GTCR		9	/* 1000BASE-T control */
#define MII_GTCR_1000_HD	(1U << 8)
#define MII_GTCR_1000_FD	(1U << 9)

/* STATUS bits. */
#define E1000_STATUS_FD		(1U << 0)
#define E1000_STATUS_LU		(1U << 1)	/* link up */
#define E1000_STATUS_SPEED_SHIFT 6
#define E1000_STATUS_SPEED_MASK	(3U << 6)	/* 0: 10, 1: 100, 2-3: 1000 Mb/s */

/* EERD: the 8254x form (82540, 82545) and the 82541/82547/82574 form. */
#define E1000_EERD_START	(1U << 0)
#define E1000_EERD_DONE		(1U << 4)
#define E1000_EERD_ADDR_SHIFT	8
#define E1000_EERD_DONE_V2	(1U << 1)
#define E1000_EERD_ADDR_SHIFT_V2 2
#define E1000_EERD_DATA_SHIFT	16

/* Interrupt causes (ICR, ICS, IMS, IMC). */
#define E1000_ICR_TXDW		(1U << 0)	/* transmit descriptor written back */
#define E1000_ICR_TXQE		(1U << 1)	/* transmit queue empty */
#define E1000_ICR_LSC		(1U << 2)	/* link status change */
#define E1000_ICR_RXSEQ		(1U << 3)	/* receive sequence error */
#define E1000_ICR_RXDMT0	(1U << 4)	/* receive descriptors low */
#define E1000_ICR_RXO		(1U << 6)	/* receiver overrun */
#define E1000_ICR_RXT0		(1U << 7)	/* receive timer */

/* RCTL bits. */
#define E1000_RCTL_EN		(1U << 1)
#define E1000_RCTL_SBP		(1U << 2)	/* store bad packets */
#define E1000_RCTL_UPE		(1U << 3)	/* unicast promiscuous */
#define E1000_RCTL_MPE		(1U << 4)	/* multicast promiscuous */
#define E1000_RCTL_LPE		(1U << 5)	/* long packets */
#define E1000_RCTL_BAM		(1U << 15)	/* accept broadcast */
#define E1000_RCTL_BSIZE_2048	(0U << 16)
#define E1000_RCTL_SECRC	(1U << 26)	/* strip the CRC */

/* TCTL bits. */
#define E1000_TCTL_EN		(1U << 1)
#define E1000_TCTL_PSP		(1U << 3)	/* pad short packets */
#define E1000_TCTL_CT_SHIFT	4		/* collision threshold */
#define E1000_TCTL_COLD_SHIFT	12		/* collision distance */
#define E1000_TCTL_RTLC		(1U << 24)

/* TIPG for copper: IPGT 10, IPGR1 8, IPGR2 6. */
#define E1000_TIPG_COPPER	(10U | (8U << 10) | (6U << 20))

#define E1000_RAH_AV		(1U << 31)	/* address valid */

/* Legacy receive descriptor. */
struct e1000_rx_desc {
	uint64_t	addr;
	uint16_t	length;
	uint16_t	csum;
	uint8_t		status;
	uint8_t		errors;
	uint16_t	special;
} __attribute__((packed));

#define E1000_RXD_STAT_DD	0x01	/* descriptor done */
#define E1000_RXD_STAT_EOP	0x02	/* end of packet */
#define E1000_RXD_STAT_IXSM	0x04	/* ignore checksum indications */
#define E1000_RXD_STAT_VP	0x08	/* VLAN tag stripped into special */
#define E1000_RXD_STAT_TCPCS	0x20	/* TCP/UDP checksum computed */
#define E1000_RXD_STAT_IPCS	0x40	/* IP checksum computed */
#define E1000_RXD_ERR_CE	0x01	/* CRC or alignment error */
#define E1000_RXD_ERR_SE	0x02	/* symbol error */
#define E1000_RXD_ERR_SEQ	0x04	/* sequence error */
#define E1000_RXD_ERR_CXE	0x10	/* carrier extension error */
#define E1000_RXD_ERR_TCPE	0x20	/* TCP/UDP checksum error */
#define E1000_RXD_ERR_IPE	0x40	/* IP checksum error */
#define E1000_RXD_ERR_RXE	0x80	/* receive data error */
#define E1000_RXD_ERR_FRAME	(E1000_RXD_ERR_CE | E1000_RXD_ERR_SE | E1000_RXD_ERR_SEQ | \
				 E1000_RXD_ERR_CXE | E1000_RXD_ERR_RXE)

/* Legacy transmit descriptor. */
struct e1000_tx_desc {
	uint64_t	addr;
	uint16_t	length;
	uint8_t		cso;
	uint8_t		cmd;
	uint8_t		status;
	uint8_t		css;
	uint16_t	special;
} __attribute__((packed));

#define E1000_TXD_CMD_EOP	0x01	/* end of packet */
#define E1000_TXD_CMD_IFCS	0x02	/* insert the CRC */
#define E1000_TXD_CMD_RS	0x08	/* report status */
#define E1000_TXD_CMD_VLE	0x40	/* insert the VLAN tag in special */
#define E1000_TXD_STAT_DD	0x01	/* descriptor done */

/*
 * Extended transmit descriptors: a TCP/IP context descriptor sets the
 * checksum offsets (and TSO's header length and MSS) that the data
 * descriptors after it use. Both are 16 bytes, like the legacy ones, and
 * share the ring with them.
 */
struct e1000_ctx_desc {
	uint8_t		ipcss;		/* IP checksum start */
	uint8_t		ipcso;		/* IP checksum offset */
	uint16_t	ipcse;		/* IP checksum end (inclusive) */
	uint8_t		tucss;		/* TCP/UDP checksum start */
	uint8_t		tucso;		/* TCP/UDP checksum offset */
	uint16_t	tucse;		/* TCP/UDP checksum end, 0: to the end */
	uint32_t	cmd_and_length;	/* PAYLEN 19:0, DTYP 23:20 (0), TUCMD 31:24 */
	uint8_t		status;
	uint8_t		hdrlen;		/* TSO: header length */
	uint16_t	mss;		/* TSO: segment size */
} __attribute__((packed));

struct e1000_data_desc {
	uint64_t	addr;
	uint32_t	cmd_and_length;	/* DTALEN 19:0, DTYP 23:20 (1), DCMD 31:24 */
	uint8_t		status;
	uint8_t		popts;		/* IXSM, TXSM */
	uint16_t	special;	/* VLAN tag */
} __attribute__((packed));

#define E1000_TXD_DTYP_CTX	(0x0U << 20)
#define E1000_TXD_DTYP_DATA	(0x1U << 20)
/* TUCMD (context) and DCMD (data) bits, in bits 31:24. */
#define E1000_TXD_TUCMD_TCP	(0x01U << 24)	/* TCP, else UDP */
#define E1000_TXD_TUCMD_IP	(0x02U << 24)	/* IPv4, else IPv6 */
#define E1000_TXD_CMD_TSE	(0x04U << 24)	/* TCP segmentation */
#define E1000_TXD_DCMD_EOP	(0x01U << 24)
#define E1000_TXD_DCMD_IFCS	(0x02U << 24)
#define E1000_TXD_DCMD_RS	(0x08U << 24)
#define E1000_TXD_DCMD_DEXT	(0x20U << 24)	/* extended descriptor */
#define E1000_TXD_DCMD_VLE	(0x40U << 24)
#define E1000_TXD_POPTS_IXSM	0x01		/* insert the IP checksum */
#define E1000_TXD_POPTS_TXSM	0x02		/* insert the TCP/UDP checksum */

#endif /* !_E1000REGS_H */
