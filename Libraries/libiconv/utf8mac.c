/*-
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
 * UTF-8-MAC: UTF-8 with the names HFS Plus stores, as macOS's iconv knows
 * it. Converting to UTF-8-MAC decomposes each character canonically (NFD,
 * in canonical order), but for those HFS Plus keeps whole (U+2000-U+2FFF,
 * U+F900-U+FAFF, U+2F800-U+2FAFF); converting from it composes them again
 * (NFC). git uses it to read file names on HFS Plus.
 *
 * The text goes by segments: a character and the combining marks after it.
 * Another encoding on the other side goes through a Citrus converter to or
 * from UTF-8. A segment that does not fit in the output is left in the
 * input (E2BIG), as iconv does with a character.
 */

#include <sys/types.h>

#include <errno.h>
#include <iconv.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include "iconv-internal.h"
#include "utf8mac.h"

struct u8m_decomp {
	uint32_t	cp, d0, d1;	/* d1 is 0 for a single character */
};
struct u8m_ccc {
	uint32_t	cp;
	uint32_t	ccc;
};
struct u8m_comp {
	uint32_t	a, b, c;	/* a + b compose to c */
};

#include "utf8mac_data.h"

#define NELEM(a)	(sizeof(a) / sizeof((a)[0]))

/* Hangul syllables decompose and compose by arithmetic. */
#define S_BASE		0xAC00
#define L_BASE		0x1100
#define V_BASE		0x1161
#define T_BASE		0x11A7
#define L_COUNT		19
#define V_COUNT		21
#define T_COUNT		28
#define N_COUNT		(V_COUNT * T_COUNT)
#define S_COUNT		(L_COUNT * N_COUNT)

/* The longest segment taken at once; a longer one is split. */
#define SEG_MAX		32
/* The longest canonical decomposition of one character. */
#define DECOMP_MAX	4

#define END_OF_INPUT	(-1)

static const char u8m_tag;

struct u8m {
	const char	*tag;		/* &u8m_tag; a Citrus handle has a
					   pointer to its shared part here */
	bool		 tomac;		/* to UTF-8-MAC, or from it */
	bool		 discard;	/* //IGNORE: skip invalid input */
	iconv_t		 inner;		/* the other side to UTF-8 (input,
					   to UTF-8-MAC) or from it (output,
					   from UTF-8-MAC); (iconv_t)-1 if
					   the other side is UTF-8 */
};

static bool
has_suffix_name(const char *name, const char *want)
{
	size_t n = strlen(want);

	return (strncasecmp(name, want, n) == 0 &&
	    (name[n] == '\0' || (name[n] == '/' && name[n + 1] == '/')));
}

int
_u8m_match(const char *name)
{

	return (has_suffix_name(name, "UTF-8-MAC") ||
	    has_suffix_name(name, "UTF8-MAC"));
}

static bool
is_utf8(const char *name)
{

	return (has_suffix_name(name, "UTF-8") || has_suffix_name(name, "UTF8"));
}

int
_u8m_is(iconv_t cd)
{

	return (*(const char * const *)(void *)cd == &u8m_tag);
}

iconv_t
_u8m_open(const char *to, const char *from)
{
	struct u8m *u;
	const char *other;

	u = calloc(1, sizeof(*u));
	if (u == NULL)
		return ((iconv_t)-1);
	u->tag = &u8m_tag;
	u->tomac = _u8m_match(to);
	u->discard = strcasestr(to, "//IGNORE") != NULL;
	u->inner = (iconv_t)-1;
	other = u->tomac ? from : to;
	if (!_u8m_match(other) && !is_utf8(other)) {
		if (u->tomac)
			u->inner = __bsd_iconv_open(
			    u->discard ? "UTF-8//IGNORE" : "UTF-8", from);
		else
			u->inner = __bsd_iconv_open(to, "UTF-8");
		if (u->inner == (iconv_t)-1) {
			free(u);
			return ((iconv_t)-1);
		}
	}
	return ((iconv_t)(void *)u);
}

int
_u8m_close(iconv_t cd)
{
	struct u8m *u = (struct u8m *)(void *)cd;

	if (u->inner != (iconv_t)-1)
		__bsd_iconv_close(u->inner);
	free(u);
	return (0);
}

/* Tables */

static uint32_t
ccc_of(uint32_t cp)
{
	size_t lo = 0, hi = NELEM(u8m_ccc), mid;

	while (lo < hi) {
		mid = (lo + hi) / 2;
		if (u8m_ccc[mid].cp == cp)
			return (u8m_ccc[mid].ccc);
		if (u8m_ccc[mid].cp < cp)
			lo = mid + 1;
		else
			hi = mid;
	}
	return (0);
}

static const struct u8m_decomp *
decomp_of(uint32_t cp)
{
	size_t lo = 0, hi = NELEM(u8m_decomp), mid;

	while (lo < hi) {
		mid = (lo + hi) / 2;
		if (u8m_decomp[mid].cp == cp)
			return (&u8m_decomp[mid]);
		if (u8m_decomp[mid].cp < cp)
			lo = mid + 1;
		else
			hi = mid;
	}
	return (NULL);
}

static bool
is_second(uint32_t cp)
{
	size_t lo = 0, hi = NELEM(u8m_second), mid;

	if ((cp >= V_BASE && cp < V_BASE + V_COUNT) ||
	    (cp > T_BASE && cp < T_BASE + T_COUNT))
		return (true);
	while (lo < hi) {
		mid = (lo + hi) / 2;
		if (u8m_second[mid] == cp)
			return (true);
		if (u8m_second[mid] < cp)
			lo = mid + 1;
		else
			hi = mid;
	}
	return (false);
}

static uint32_t
compose_pair(uint32_t a, uint32_t b)
{
	size_t lo = 0, hi = NELEM(u8m_comp), mid;

	if (a >= L_BASE && a < L_BASE + L_COUNT &&
	    b >= V_BASE && b < V_BASE + V_COUNT)
		return (S_BASE + ((a - L_BASE) * V_COUNT + (b - V_BASE)) *
		    T_COUNT);
	if (a >= S_BASE && a < S_BASE + S_COUNT &&
	    (a - S_BASE) % T_COUNT == 0 &&
	    b > T_BASE && b < T_BASE + T_COUNT)
		return (a + (b - T_BASE));
	while (lo < hi) {
		mid = (lo + hi) / 2;
		if (u8m_comp[mid].a == a && u8m_comp[mid].b == b)
			return (u8m_comp[mid].c);
		if (u8m_comp[mid].a < a ||
		    (u8m_comp[mid].a == a && u8m_comp[mid].b < b))
			lo = mid + 1;
		else
			hi = mid;
	}
	return (0);
}

/* Normalization */

static size_t
decompose(uint32_t cp, uint32_t *out)
{
	const struct u8m_decomp *d;
	uint32_t s;
	size_t n;

	if (cp >= S_BASE && cp < S_BASE + S_COUNT) {
		s = cp - S_BASE;
		out[0] = L_BASE + s / N_COUNT;
		out[1] = V_BASE + (s % N_COUNT) / T_COUNT;
		if (s % T_COUNT == 0)
			return (2);
		out[2] = T_BASE + s % T_COUNT;
		return (3);
	}
	d = decomp_of(cp);
	if (d == NULL) {
		out[0] = cp;
		return (1);
	}
	n = decompose(d->d0, out);
	if (d->d1 != 0)
		n += decompose(d->d1, out + n);
	return (n);
}

/* Puts each run of combining marks in canonical order (a stable sort). */
static void
reorder(uint32_t *cps, size_t n)
{
	size_t i, j;
	uint32_t c, cc;

	for (i = 1; i < n; i++) {
		c = cps[i];
		cc = ccc_of(c);
		if (cc == 0)
			continue;
		for (j = i; j > 0 && ccc_of(cps[j - 1]) > cc; j--)
			cps[j] = cps[j - 1];
		cps[j] = c;
	}
}

static size_t
decompose_segment(const uint32_t *seg, size_t n, uint32_t *out)
{
	size_t i, m = 0;

	for (i = 0; i < n; i++)
		m += decompose(seg[i], out + m);
	reorder(out, m);
	return (m);
}

static size_t
compose_segment(const uint32_t *seg, size_t n, uint32_t *out)
{
	size_t i, m, w, starter;
	uint32_t c, cc, comp, last;

	m = decompose_segment(seg, n, out);
	if (m == 0)
		return (0);
	starter = 0;
	/* Nothing composes onto a segment that starts with a mark. */
	last = ccc_of(out[0]) == 0 ? 0 : 256;
	w = 1;
	for (i = 1; i < m; i++) {
		c = out[i];
		cc = ccc_of(c);
		comp = last == 256 ? 0 : compose_pair(out[starter], c);
		if (comp != 0 && (last == 0 || last < cc)) {
			out[starter] = comp;
			continue;
		}
		if (cc == 0) {
			starter = w;
			last = 0;
		} else
			last = cc;
		out[w++] = c;
	}
	return (w);
}

/* Whether cp goes on the segment before it. */
static bool
continues(const struct u8m *u, uint32_t cp)
{
	uint32_t d[DECOMP_MAX * 4];

	if (u->tomac) {
		decompose(cp, d);
		return (ccc_of(d[0]) != 0);
	}
	return (ccc_of(cp) != 0 || is_second(cp));
}

/* UTF-8 */

static int
utf8_decode(const unsigned char *p, size_t n, uint32_t *cp, size_t *len)
{
	uint32_t c, min;
	size_t i, l;

	if (n == 0)
		return (EINVAL);
	c = p[0];
	if (c < 0x80) {
		*cp = c;
		*len = 1;
		return (0);
	} else if (c >= 0xC2 && c <= 0xDF) {
		l = 2; c &= 0x1F; min = 0x80;
	} else if (c >= 0xE0 && c <= 0xEF) {
		l = 3; c &= 0x0F; min = 0x800;
	} else if (c >= 0xF0 && c <= 0xF4) {
		l = 4; c &= 0x07; min = 0x10000;
	} else
		return (EILSEQ);
	for (i = 1; i < l; i++) {
		if (i >= n)
			return (EINVAL);
		if ((p[i] & 0xC0) != 0x80)
			return (EILSEQ);
		c = (c << 6) | (p[i] & 0x3F);
	}
	if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
		return (EILSEQ);
	*cp = c;
	*len = l;
	return (0);
}

static size_t
utf8_encode(uint32_t c, unsigned char *p)
{

	if (c < 0x80) {
		p[0] = c;
		return (1);
	} else if (c < 0x800) {
		p[0] = 0xC0 | (c >> 6);
		p[1] = 0x80 | (c & 0x3F);
		return (2);
	} else if (c < 0x10000) {
		p[0] = 0xE0 | (c >> 12);
		p[1] = 0x80 | ((c >> 6) & 0x3F);
		p[2] = 0x80 | (c & 0x3F);
		return (3);
	}
	p[0] = 0xF0 | (c >> 18);
	p[1] = 0x80 | ((c >> 12) & 0x3F);
	p[2] = 0x80 | ((c >> 6) & 0x3F);
	p[3] = 0x80 | (c & 0x3F);
	return (4);
}

/* Reading and writing code points */

/*
 * Reads one character. From another encoding the converter is given room
 * for 1, then 2, 3 and 4 bytes of UTF-8, so that it gives one character.
 */
static int
read_cp(struct u8m *u, char **in, size_t *left, uint32_t *cp, size_t *irrev)
{
	unsigned char buf[4];
	char *ip, *op;
	size_t il, ol, len, r, room;
	int e;

	if (*left == 0)
		return (END_OF_INPUT);
	if (!u->tomac || u->inner == (iconv_t)-1) {
		e = utf8_decode((unsigned char *)*in, *left, cp, &len);
		if (e == 0) {
			*in += len;
			*left -= len;
		}
		return (e);
	}
	for (room = 1; room <= sizeof(buf); room++) {
		ip = *in;
		il = *left;
		op = (char *)buf;
		ol = room;
		r = __bsd_iconv(u->inner, &ip, &il, &op, &ol);
		e = r == (size_t)-1 ? errno : 0;
		if (op != (char *)buf) {
			if (utf8_decode(buf, room - ol, cp, &len) != 0)
				return (EILSEQ);
			*in = ip;
			*left = il;
			if (e == 0)
				*irrev += r;
			return (0);
		}
		if (e == 0) {
			/* Input that makes no character (a shift sequence). */
			*in = ip;
			*left = il;
			return (END_OF_INPUT);
		}
		if (e != E2BIG)
			return (e);
	}
	return (EILSEQ);
}

static int
write_cps(struct u8m *u, const uint32_t *cps, size_t n, char **out,
    size_t *outleft, size_t *irrev)
{
	unsigned char buf[SEG_MAX * DECOMP_MAX * 4];
	char *ip, *op;
	size_t i, len, il, ol, r;

	for (i = 0, len = 0; i < n; i++)
		len += utf8_encode(cps[i], buf + len);
	if (u->tomac || u->inner == (iconv_t)-1) {
		if (len > *outleft)
			return (E2BIG);
		memcpy(*out, buf, len);
		*out += len;
		*outleft -= len;
		return (0);
	}
	ip = (char *)buf;
	il = len;
	op = *out;
	ol = *outleft;
	r = __bsd_iconv(u->inner, &ip, &il, &op, &ol);
	if (r == (size_t)-1)
		return (errno);
	*out = op;
	*outleft = ol;
	*irrev += r;
	return (0);
}

size_t
_u8m_iconv(iconv_t cd, char **in, size_t *inleft, char **out,
    size_t *outleft)
{
	struct u8m *u = (struct u8m *)(void *)cd;
	uint32_t seg[SEG_MAX], res[SEG_MAX * DECOMP_MAX], cp;
	char *seg_in, *p;
	size_t seg_left, l, n, m, irrev = 0;
	int e;

	if (in == NULL || *in == NULL) {
		if (u->inner != (iconv_t)-1)
			return (__bsd_iconv(u->inner, NULL, NULL, out, outleft));
		return (0);
	}
	for (;;) {
		seg_in = *in;
		seg_left = *inleft;
		e = read_cp(u, in, inleft, &cp, &irrev);
		if (e == END_OF_INPUT)
			break;
		if (e == EILSEQ && u->discard &&
		    (!u->tomac || u->inner == (iconv_t)-1)) {
			*in = seg_in + 1;
			*inleft = seg_left - 1;
			continue;
		}
		if (e != 0) {
			*in = seg_in;
			*inleft = seg_left;
			errno = e;
			return ((size_t)-1);
		}
		n = 0;
		seg[n++] = cp;
		while (n < SEG_MAX) {
			p = *in;
			l = *inleft;
			if (read_cp(u, in, inleft, &cp, &irrev) != 0 ||
			    !continues(u, cp)) {
				*in = p;
				*inleft = l;
				break;
			}
			seg[n++] = cp;
		}
		m = u->tomac ? decompose_segment(seg, n, res) :
		    compose_segment(seg, n, res);
		e = write_cps(u, res, m, out, outleft, &irrev);
		if (e != 0) {
			*in = seg_in;
			*inleft = seg_left;
			errno = e;
			return ((size_t)-1);
		}
	}
	return (irrev);
}

int
_u8m_iconvctl(iconv_t cd, int request, void *argument)
{
	struct u8m *u = (struct u8m *)(void *)cd;
	int *i = (int *)argument;

	switch (request) {
	case ICONV_TRIVIALP:
		*i = 0;
		return (0);
	case ICONV_GET_DISCARD_ILSEQ:
		*i = u->discard ? 1 : 0;
		return (0);
	case ICONV_SET_DISCARD_ILSEQ:
		u->discard = *i != 0;
		break;
	}
	if (u->inner != (iconv_t)-1)
		return (__bsd_iconvctl(u->inner, request, argument));
	switch (request) {
	case ICONV_SET_DISCARD_ILSEQ:
	case ICONV_SET_HOOKS:
		return (0);
	case ICONV_GET_TRANSLITERATE:
		*i = 1;
		return (0);
	case ICONV_SET_TRANSLITERATE:
		return (*i == 1 ? 0 : -1);
	case ICONV_GET_ILSEQ_INVALID:
		*i = 0;
		return (0);
	case ICONV_SET_ILSEQ_INVALID:
		return (0);
	}
	errno = EINVAL;
	return (-1);
}
