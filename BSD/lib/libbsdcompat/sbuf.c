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
 * sbuf(9) string buffers for commands, as <sys/sbuf.h> declares them:
 * the SDK has the header but Darwin's libc has no sbuf. A buffer keeps
 * room for its terminating NUL; a fixed one that fills up is marked
 * overflowed and keeps what fit, an SBUF_AUTOEXTEND one grows.
 */

#include <sys/types.h>
#include <sys/sbuf.h>

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define	SBUF_MINSIZE	64

static int
sbuf_grow(struct sbuf *s, size_t need)
{
	size_t size;
	char *buf;

	if ((s->s_flags & SBUF_AUTOEXTEND) == 0)
		return (-1);
	size = s->s_size > 0 ? (size_t)s->s_size : SBUF_MINSIZE;
	while (size < need) {
		if (size > (size_t)__INT_MAX__ / 2)
			return (-1);
		size *= 2;
	}
	if (s->s_flags & SBUF_DYNAMIC)
		buf = realloc(s->s_buf, size);
	else if ((buf = malloc(size)) != NULL && s->s_buf != NULL)
		memcpy(buf, s->s_buf, s->s_len);
	if (buf == NULL)
		return (-1);
	s->s_buf = buf;
	s->s_size = (int)size;
	s->s_flags |= SBUF_DYNAMIC;
	return (0);
}

struct sbuf *
sbuf_new(struct sbuf *s, char *buf, int length, int flags)
{
	int dynstruct = 0;

	/* A caller's buffer needs room for at least the NUL. */
	if (length < 0 || (buf != NULL && length == 0))
		return (NULL);
	if (s == NULL) {
		if ((s = calloc(1, sizeof(*s))) == NULL)
			return (NULL);
		dynstruct = SBUF_DYNSTRUCT;
	} else
		memset(s, 0, sizeof(*s));
	s->s_flags = (flags & SBUF_USRFLAGMSK) | dynstruct;
	s->s_size = length;
	s->s_buf = buf;
	if (buf == NULL) {
		if (length == 0)
			s->s_size = SBUF_MINSIZE;
		if ((s->s_buf = malloc(s->s_size)) == NULL) {
			if (dynstruct)
				free(s);
			return (NULL);
		}
		s->s_flags |= SBUF_DYNAMIC;
	}
	s->s_buf[0] = '\0';
	return (s);
}

void
sbuf_clear(struct sbuf *s)
{
	s->s_flags &= ~(SBUF_FINISHED | SBUF_OVERFLOWED);
	s->s_len = 0;
}

int
sbuf_setpos(struct sbuf *s, int pos)
{
	if (pos < 0 || pos > s->s_len)
		return (-1);
	s->s_len = pos;
	return (0);
}

int
sbuf_bcat(struct sbuf *s, const void *data, size_t len)
{
	size_t room;

	if (s->s_flags & SBUF_OVERFLOWED)
		return (-1);
	if ((size_t)s->s_len + len + 1 > (size_t)s->s_size &&
	    sbuf_grow(s, (size_t)s->s_len + len + 1) == -1) {
		/* Keep what fits, as FreeBSD's fixed buffers do. */
		room = s->s_size > s->s_len ? s->s_size - s->s_len - 1 : 0;
		memcpy(s->s_buf + s->s_len, data, room);
		s->s_len += (int)room;
		s->s_flags |= SBUF_OVERFLOWED;
		return (-1);
	}
	memcpy(s->s_buf + s->s_len, data, len);
	s->s_len += (int)len;
	return (0);
}

int
sbuf_bcpy(struct sbuf *s, const void *data, size_t len)
{
	sbuf_clear(s);
	return (sbuf_bcat(s, data, len));
}

int
sbuf_cat(struct sbuf *s, const char *str)
{
	return (sbuf_bcat(s, str, strlen(str)));
}

int
sbuf_cpy(struct sbuf *s, const char *str)
{
	sbuf_clear(s);
	return (sbuf_cat(s, str));
}

int
sbuf_vprintf(struct sbuf *s, const char *fmt, va_list ap)
{
	va_list aq;
	char *str;
	int len, error;

	if (s->s_flags & SBUF_OVERFLOWED)
		return (-1);
	va_copy(aq, ap);
	len = vasprintf(&str, fmt, aq);
	va_end(aq);
	if (len < 0) {
		s->s_flags |= SBUF_OVERFLOWED;
		return (-1);
	}
	error = sbuf_bcat(s, str, (size_t)len);
	free(str);
	return (error);
}

int
sbuf_printf(struct sbuf *s, const char *fmt, ...)
{
	va_list ap;
	int error;

	va_start(ap, fmt);
	error = sbuf_vprintf(s, fmt, ap);
	va_end(ap);
	return (error);
}

int
sbuf_putc(struct sbuf *s, int c)
{
	char ch = (char)c;

	return (sbuf_bcat(s, &ch, 1));
}

int
sbuf_trim(struct sbuf *s)
{
	if (s->s_flags & SBUF_OVERFLOWED)
		return (-1);
	while (s->s_len > 0 &&
	    isspace((unsigned char)s->s_buf[s->s_len - 1]))
		s->s_len--;
	return (0);
}

int
sbuf_overflowed(struct sbuf *s)
{
	return ((s->s_flags & SBUF_OVERFLOWED) != 0);
}

void
sbuf_finish(struct sbuf *s)
{
	s->s_buf[s->s_len] = '\0';
	s->s_flags |= SBUF_FINISHED;
}

char *
sbuf_data(struct sbuf *s)
{
	return (s->s_buf);
}

int
sbuf_len(struct sbuf *s)
{
	if (s->s_flags & SBUF_OVERFLOWED)
		return (-1);
	return (s->s_len);
}

int
sbuf_done(struct sbuf *s)
{
	return ((s->s_flags & SBUF_FINISHED) != 0);
}

void
sbuf_delete(struct sbuf *s)
{
	int dynstruct = s->s_flags & SBUF_DYNSTRUCT;

	if (s->s_flags & SBUF_DYNAMIC)
		free(s->s_buf);
	memset(s, 0, sizeof(*s));
	if (dynstruct)
		free(s);
}
