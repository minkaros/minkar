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
 * <compression.h>: libcompression's interface, ours, after its public
 * documentation: buffer and stream compression with ZLIB (raw DEFLATE),
 * LZMA (xz) and LZFSE.
 */

#ifndef __COMPRESSION_HEADER__
#define __COMPRESSION_HEADER__

#include <stddef.h>
#include <stdint.h>
#include <sys/cdefs.h>
#include <Availability.h>

__BEGIN_DECLS

typedef enum {
	COMPRESSION_LZ4 = 0x100,
	COMPRESSION_ZLIB = 0x205,
	COMPRESSION_LZMA = 0x306,
	COMPRESSION_LZ4_RAW = 0x101,
	COMPRESSION_BROTLI = 0xB02,
	COMPRESSION_LZFSE = 0x801,
	COMPRESSION_LZBITMAP = 0x702,
} compression_algorithm;

size_t compression_encode_scratch_buffer_size(compression_algorithm algorithm)
    __API_AVAILABLE(macos(10.11));
size_t compression_encode_buffer(uint8_t *__restrict dst_buffer,
    size_t dst_size, const uint8_t *__restrict src_buffer, size_t src_size,
    void *__restrict scratch_buffer, compression_algorithm algorithm)
    __API_AVAILABLE(macos(10.11));
size_t compression_decode_scratch_buffer_size(compression_algorithm algorithm)
    __API_AVAILABLE(macos(10.11));
size_t compression_decode_buffer(uint8_t *__restrict dst_buffer,
    size_t dst_size, const uint8_t *__restrict src_buffer, size_t src_size,
    void *__restrict scratch_buffer, compression_algorithm algorithm)
    __API_AVAILABLE(macos(10.11));

typedef struct {
	uint8_t *dst_ptr;
	size_t dst_size;
	const uint8_t *src_ptr;
	size_t src_size;
	void *state;
} compression_stream;

typedef enum {
	COMPRESSION_STREAM_ENCODE = 0,
	COMPRESSION_STREAM_DECODE = 1,
} compression_stream_operation;

typedef enum {
	COMPRESSION_STREAM_FINALIZE = 0x0001,
} compression_stream_flags;

typedef enum {
	COMPRESSION_STATUS_OK = 0,
	COMPRESSION_STATUS_ERROR = -1,
	COMPRESSION_STATUS_END = 1,
} compression_status;

compression_status compression_stream_init(compression_stream *stream,
    compression_stream_operation operation, compression_algorithm algorithm)
    __API_AVAILABLE(macos(10.11));
compression_status compression_stream_process(compression_stream *stream,
    int flags) __API_AVAILABLE(macos(10.11));
compression_status compression_stream_destroy(compression_stream *stream)
    __API_AVAILABLE(macos(10.11));

__END_DECLS

#endif /* __COMPRESSION_HEADER__ */
