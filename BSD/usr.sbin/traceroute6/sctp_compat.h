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
 * The SCTP packet formats traceroute6 -S sends (RFC 9260, section 3),
 * which Darwin's SDK has no header for: the common header, a chunk
 * header, a parameter header and the INIT chunk.
 */

#ifndef _SCTP_COMPAT_H
#define _SCTP_COMPAT_H

#include <stdint.h>

struct sctphdr {
	uint16_t	src_port;
	uint16_t	dest_port;
	uint32_t	v_tag;
	uint32_t	checksum;
} __attribute__((packed));

struct sctp_chunkhdr {
	uint8_t		chunk_type;
	uint8_t		chunk_flags;
	uint16_t	chunk_length;
} __attribute__((packed));

struct sctp_paramhdr {
	uint16_t	param_type;
	uint16_t	param_length;
} __attribute__((packed));

struct sctp_init {
	uint32_t	initiate_tag;
	uint32_t	a_rwnd;
	uint16_t	num_outbound_streams;
	uint16_t	num_inbound_streams;
	uint32_t	initial_tsn;
} __attribute__((packed));

struct sctp_init_chunk {
	struct sctp_chunkhdr	ch;
	struct sctp_init	init;
} __attribute__((packed));

/* chunk types */
#define	SCTP_INITIATION		0x01
#define	SCTP_SHUTDOWN_ACK	0x08
#define	SCTP_PAD_CHUNK		0x84	/* RFC 4820 */

/* parameter types */
#define	SCTP_PAD		0x8005	/* RFC 4820 */

#endif /* !_SCTP_COMPAT_H */
