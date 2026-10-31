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
 * libcompression, ours: ZLIB as raw DEFLATE (zlib), LZMA as xz (liblzma),
 * LZFSE (the open lzfse, in dist/). Streams of ZLIB and LZMA go through
 * as they come; LZFSE has no stream interface of its own, so a stream
 * keeps its input and works on it whole, at the end. LZ4, LZ4_RAW,
 * Brotli and LZBITMAP are not there yet: noisy stubs, errors.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>
#include <lzma.h>
#include <minkar/stub.h>

#include "compression.h"
#include "lzfse.h"

#define ZLIB_LEVEL	5
#define LZMA_PRESET	6

static void
unsupported(compression_algorithm algorithm)
{
	switch (algorithm) {
	case COMPRESSION_LZ4:
		_minkar_stub("compression: COMPRESSION_LZ4",
		    __builtin_return_address(0));
		break;
	case COMPRESSION_LZ4_RAW:
		_minkar_stub("compression: COMPRESSION_LZ4_RAW",
		    __builtin_return_address(0));
		break;
	case COMPRESSION_BROTLI:
		_minkar_stub("compression: COMPRESSION_BROTLI",
		    __builtin_return_address(0));
		break;
	case COMPRESSION_LZBITMAP:
		_minkar_stub("compression: COMPRESSION_LZBITMAP",
		    __builtin_return_address(0));
		break;
	default:
		break;
	}
}

static bool
supported(compression_algorithm algorithm)
{
	return algorithm == COMPRESSION_ZLIB || algorithm == COMPRESSION_LZMA ||
	    algorithm == COMPRESSION_LZFSE;
}

size_t
compression_encode_scratch_buffer_size(compression_algorithm algorithm)
{
	if (algorithm == COMPRESSION_LZFSE)
		return lzfse_encode_scratch_size();
	unsupported(algorithm);
	return supported(algorithm) ? 1 : 0;
}

size_t
compression_decode_scratch_buffer_size(compression_algorithm algorithm)
{
	if (algorithm == COMPRESSION_LZFSE)
		return lzfse_decode_scratch_size();
	unsupported(algorithm);
	return supported(algorithm) ? 1 : 0;
}

/* ---- streams ---- */

struct whole {			/* LZFSE: the input, kept; the output, made */
	uint8_t *in;
	size_t in_len, in_cap;
	uint8_t *out;
	size_t out_len, out_off;
	bool made;
};

struct state {
	compression_stream_operation op;
	compression_algorithm alg;
	union {
		z_stream z;
		lzma_stream x;
		struct whole w;
	} u;
};

static compression_status
whole_take(struct whole *w, compression_stream *s)
{
	if (s->src_size == 0)
		return COMPRESSION_STATUS_OK;
	if (w->in_len + s->src_size > w->in_cap) {
		size_t cap = w->in_cap ? w->in_cap : 65536;
		uint8_t *p;

		while (cap < w->in_len + s->src_size)
			cap *= 2;
		if ((p = realloc(w->in, cap)) == NULL)
			return COMPRESSION_STATUS_ERROR;
		w->in = p;
		w->in_cap = cap;
	}
	memcpy(w->in + w->in_len, s->src_ptr, s->src_size);
	w->in_len += s->src_size;
	s->src_ptr += s->src_size;
	s->src_size = 0;
	return COMPRESSION_STATUS_OK;
}

/* Out of what was made, as much as the caller has room for. */
static compression_status
whole_give(struct whole *w, compression_stream *s)
{
	size_t n = w->out_len - w->out_off;

	if (n > s->dst_size)
		n = s->dst_size;
	memcpy(s->dst_ptr, w->out + w->out_off, n);
	w->out_off += n;
	s->dst_ptr += n;
	s->dst_size -= n;
	return w->out_off == w->out_len ? COMPRESSION_STATUS_END :
	    COMPRESSION_STATUS_OK;
}

static compression_status
whole_make(struct whole *w, bool encode)
{
	size_t cap = encode ? w->in_len + w->in_len / 8 + 4096 :
	    w->in_len * 4 + 4096;

	for (;;) {
		uint8_t *p = realloc(w->out, cap);
		size_t n;

		if (p == NULL)
			return COMPRESSION_STATUS_ERROR;
		w->out = p;
		n = encode ? lzfse_encode_buffer(w->out, cap, w->in, w->in_len,
		    NULL) : lzfse_decode_buffer(w->out, cap, w->in, w->in_len,
		    NULL);
		if (encode ? n != 0 : n < cap) {
			w->out_len = n;
			w->made = true;
			return COMPRESSION_STATUS_OK;
		}
		if (cap > ((size_t)1 << 40))
			return COMPRESSION_STATUS_ERROR;
		cap *= 2;
	}
}

static bool
lzfse_ended(const struct whole *w)
{
	return w->in_len >= 4 &&
	    memcmp(w->in + w->in_len - 4, "bvx$", 4) == 0;
}

compression_status
compression_stream_init(compression_stream *stream,
    compression_stream_operation op, compression_algorithm alg)
{
	struct state *st;

	if (stream == NULL)
		return COMPRESSION_STATUS_ERROR;
	stream->dst_ptr = NULL;
	stream->dst_size = 0;
	stream->src_ptr = NULL;
	stream->src_size = 0;
	stream->state = NULL;
	if (!supported(alg)) {
		unsupported(alg);
		return COMPRESSION_STATUS_ERROR;
	}
	if (op != COMPRESSION_STREAM_ENCODE && op != COMPRESSION_STREAM_DECODE)
		return COMPRESSION_STATUS_ERROR;
	if ((st = calloc(1, sizeof(*st))) == NULL)
		return COMPRESSION_STATUS_ERROR;
	st->op = op;
	st->alg = alg;
	if (alg == COMPRESSION_ZLIB) {
		int r = op == COMPRESSION_STREAM_ENCODE ?
		    deflateInit2(&st->u.z, ZLIB_LEVEL, Z_DEFLATED, -15, 8,
		    Z_DEFAULT_STRATEGY) : inflateInit2(&st->u.z, -15);

		if (r != Z_OK) {
			free(st);
			return COMPRESSION_STATUS_ERROR;
		}
	} else if (alg == COMPRESSION_LZMA) {
		lzma_stream init = LZMA_STREAM_INIT;
		lzma_ret r;

		st->u.x = init;
		r = op == COMPRESSION_STREAM_ENCODE ?
		    lzma_easy_encoder(&st->u.x, LZMA_PRESET, LZMA_CHECK_CRC64) :
		    lzma_stream_decoder(&st->u.x, UINT64_MAX, 0);
		if (r != LZMA_OK) {
			free(st);
			return COMPRESSION_STATUS_ERROR;
		}
	}
	stream->state = st;
	return COMPRESSION_STATUS_OK;
}

compression_status
compression_stream_process(compression_stream *stream, int flags)
{
	struct state *st = stream ? stream->state : NULL;
	bool finalize = (flags & COMPRESSION_STREAM_FINALIZE) != 0;

	if (st == NULL)
		return COMPRESSION_STATUS_ERROR;
	if (st->alg == COMPRESSION_ZLIB) {
		z_stream *z = &st->u.z;
		int r;

		z->next_in = (Bytef *)(uintptr_t)stream->src_ptr;
		z->avail_in = (uInt)(stream->src_size > UINT32_MAX ? UINT32_MAX :
		    stream->src_size);
		z->next_out = stream->dst_ptr;
		z->avail_out = (uInt)(stream->dst_size > UINT32_MAX ?
		    UINT32_MAX : stream->dst_size);
		{
			size_t in0 = z->avail_in, out0 = z->avail_out;

			r = st->op == COMPRESSION_STREAM_ENCODE ?
			    deflate(z, finalize ? Z_FINISH : Z_NO_FLUSH) :
			    inflate(z, Z_NO_FLUSH);
			stream->src_ptr += in0 - z->avail_in;
			stream->src_size -= in0 - z->avail_in;
			stream->dst_ptr += out0 - z->avail_out;
			stream->dst_size -= out0 - z->avail_out;
		}
		if (r == Z_STREAM_END)
			return COMPRESSION_STATUS_END;
		if (r == Z_OK || r == Z_BUF_ERROR)
			return COMPRESSION_STATUS_OK;
		return COMPRESSION_STATUS_ERROR;
	}
	if (st->alg == COMPRESSION_LZMA) {
		lzma_stream *x = &st->u.x;
		size_t in0 = stream->src_size, out0 = stream->dst_size;
		lzma_ret r;

		x->next_in = stream->src_ptr;
		x->avail_in = stream->src_size;
		x->next_out = stream->dst_ptr;
		x->avail_out = stream->dst_size;
		r = lzma_code(x, st->op == COMPRESSION_STREAM_ENCODE && finalize ?
		    LZMA_FINISH : LZMA_RUN);
		stream->src_ptr += in0 - x->avail_in;
		stream->src_size = x->avail_in;
		stream->dst_ptr += out0 - x->avail_out;
		stream->dst_size = x->avail_out;
		if (r == LZMA_STREAM_END)
			return COMPRESSION_STATUS_END;
		if (r == LZMA_OK || r == LZMA_BUF_ERROR)
			return COMPRESSION_STATUS_OK;
		return COMPRESSION_STATUS_ERROR;
	}

	/* LZFSE */
	struct whole *w = &st->u.w;

	if (!w->made) {
		if (whole_take(w, stream) != COMPRESSION_STATUS_OK)
			return COMPRESSION_STATUS_ERROR;
		if (st->op == COMPRESSION_STREAM_ENCODE ? !finalize :
		    !lzfse_ended(w))
			return COMPRESSION_STATUS_OK;
		if (whole_make(w, st->op == COMPRESSION_STREAM_ENCODE) !=
		    COMPRESSION_STATUS_OK)
			return COMPRESSION_STATUS_ERROR;
	}
	return whole_give(w, stream);
}

compression_status
compression_stream_destroy(compression_stream *stream)
{
	struct state *st = stream ? stream->state : NULL;

	if (st == NULL)
		return COMPRESSION_STATUS_ERROR;
	if (st->alg == COMPRESSION_ZLIB) {
		if (st->op == COMPRESSION_STREAM_ENCODE)
			deflateEnd(&st->u.z);
		else
			inflateEnd(&st->u.z);
	} else if (st->alg == COMPRESSION_LZMA)
		lzma_end(&st->u.x);
	else {
		free(st->u.w.in);
		free(st->u.w.out);
	}
	free(st);
	stream->state = NULL;
	return COMPRESSION_STATUS_OK;
}

/* ---- buffers: a stream, at once ---- */

static size_t
whole_buffer(compression_stream_operation op, uint8_t *dst, size_t dst_size,
    const uint8_t *src, size_t src_size, compression_algorithm alg)
{
	compression_stream s;
	compression_status r;
	size_t made;

	if (alg == COMPRESSION_LZFSE)
		return op == COMPRESSION_STREAM_ENCODE ?
		    lzfse_encode_buffer(dst, dst_size, src, src_size, NULL) :
		    lzfse_decode_buffer(dst, dst_size, src, src_size, NULL);
	if (compression_stream_init(&s, op, alg) != COMPRESSION_STATUS_OK)
		return 0;
	s.src_ptr = src;
	s.src_size = src_size;
	s.dst_ptr = dst;
	s.dst_size = dst_size;
	do {
		size_t before = s.dst_size;

		r = compression_stream_process(&s, COMPRESSION_STREAM_FINALIZE);
		if (r == COMPRESSION_STATUS_OK && s.dst_size == before &&
		    (s.dst_size == 0 || s.src_size == 0))
			break;
	} while (r == COMPRESSION_STATUS_OK);
	made = dst_size - s.dst_size;
	compression_stream_destroy(&s);
	if (r == COMPRESSION_STATUS_ERROR)
		return 0;
	/* Encoding into too small a buffer fails; decoding gives what fits. */
	if (op == COMPRESSION_STREAM_ENCODE && r != COMPRESSION_STATUS_END)
		return 0;
	return made;
}

size_t
compression_encode_buffer(uint8_t *__restrict dst, size_t dst_size,
    const uint8_t *__restrict src, size_t src_size, void *__restrict scratch,
    compression_algorithm alg)
{
	(void)scratch;
	if (!supported(alg)) {
		unsupported(alg);
		return 0;
	}
	return whole_buffer(COMPRESSION_STREAM_ENCODE, dst, dst_size, src,
	    src_size, alg);
}

size_t
compression_decode_buffer(uint8_t *__restrict dst, size_t dst_size,
    const uint8_t *__restrict src, size_t src_size, void *__restrict scratch,
    compression_algorithm alg)
{
	(void)scratch;
	if (!supported(alg)) {
		unsupported(alg);
		return 0;
	}
	return whole_buffer(COMPRESSION_STREAM_DECODE, dst, dst_size, src,
	    src_size, alg);
}
