/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * POSIX extended regular expressions for sandbox profiles, searched in
 * a subject. Compiled to a Thompson automaton in fixed arrays and run
 * over all its states at once: no recursion and no allocation, so the
 * kernel's sandbox policy can use it too, in time linear in the subject.
 *
 * Supported: literals, '.', bracket expressions (ranges, negation,
 * [:class:]), '^', '$', groups, '|', '*', '+', '?', {m}, {m,}, {m,n}
 * (n up to 16), and '\' escapes.
 */

#include "sbblob.h"

#define MAX_STATES 512
#define MAX_FRAGS 128

enum { ST_CHAR = 1, ST_ANY, ST_CLASS, ST_SPLIT, ST_BOL, ST_EOL, ST_MATCH };

struct state {
	uint8_t type;
	uint8_t c;
	uint16_t cls;		/* class index */
	int16_t out, out1;
};

struct regex {
	struct state states[MAX_STATES];
	int16_t links[MAX_STATES * 2];	/* dangling lists: next slot code, -1 ends */
	uint8_t classes[64][32];
	int nstates, nclasses;
	int error;
};

/* A fragment: its start and its dangling exits, a list of slot codes
   (state index * 2, + 1 for out1) linked through re->links. */
struct frag {
	int start;
	int first, last;	/* dangling list: encoded slots */
};

static int new_state(struct regex *re, int type, int c) {
	if (re->nstates >= MAX_STATES) {
		re->error = 1;
		return 0;
	}
	struct state *s = &re->states[re->nstates];
	s->type = (uint8_t)type;
	s->c = (uint8_t)c;
	s->cls = 0;
	s->out = -1;
	s->out1 = -1;
	re->links[re->nstates * 2] = -1;
	re->links[re->nstates * 2 + 1] = -1;
	return re->nstates++;
}

static int16_t *slot(struct regex *re, int code) {
	struct state *s = &re->states[code >> 1];
	return (code & 1) ? &s->out1 : &s->out;
}

static void patch(struct regex *re, int list, int target) {
	while (list != -1) {
		*slot(re, list) = (int16_t)target;
		list = re->links[list];
	}
}

static int append(struct regex *re, int a, int b) {
	if (a == -1)
		return b;
	int p = a;
	while (re->links[p] != -1)
		p = re->links[p];
	re->links[p] = (int16_t)b;
	return a;
}

static struct frag single(struct regex *re, int type, int c) {
	struct frag f;
	f.start = new_state(re, type, c);
	f.first = f.start * 2;
	f.last = f.first;
	re->links[f.first] = -1;
	return f;
}

/* ---- Parsing: a recursive-descent parser over an explicit stack ---- */

struct parser {
	struct regex *re;
	const char *p;
};

static int class_bit(const uint8_t *cls, unsigned c) {
	return (cls[c >> 3] >> (c & 7)) & 1;
}

static void class_set(uint8_t *cls, unsigned c) {
	cls[c >> 3] |= (uint8_t)(1 << (c & 7));
}

static int is_in_named_class(const char *name, size_t len, unsigned c) {
#define IS(n) (len == sizeof(n) - 1 && __builtin_memcmp(name, n, len) == 0)
	if (IS("alpha")) return (c | 32) >= 'a' && (c | 32) <= 'z';
	if (IS("digit")) return c >= '0' && c <= '9';
	if (IS("alnum")) return ((c | 32) >= 'a' && (c | 32) <= 'z') || (c >= '0' && c <= '9');
	if (IS("upper")) return c >= 'A' && c <= 'Z';
	if (IS("lower")) return c >= 'a' && c <= 'z';
	if (IS("space")) return c == ' ' || (c >= 9 && c <= 13);
	if (IS("xdigit")) return (c >= '0' && c <= '9') || ((c | 32) >= 'a' && (c | 32) <= 'f');
	if (IS("punct")) return c > 32 && c < 127 && !(((c | 32) >= 'a' && (c | 32) <= 'z') || (c >= '0' && c <= '9'));
	if (IS("print")) return c >= 32 && c < 127;
	if (IS("cntrl")) return c < 32 || c == 127;
	return 0;
#undef IS
}

static struct frag parse_class(struct parser *ps) {
	struct regex *re = ps->re;
	struct frag f = single(re, ST_CLASS, 0);
	if (re->nclasses >= 64) {
		re->error = 1;
		return f;
	}
	uint8_t *cls = re->classes[re->nclasses];
	for (int i = 0; i < 32; i++)
		cls[i] = 0;
	re->states[f.start].cls = (uint16_t)re->nclasses++;
	int negate = 0;
	if (*ps->p == '^') {
		negate = 1;
		ps->p++;
	}
	int first = 1;
	while (*ps->p != '\0' && (*ps->p != ']' || first)) {
		first = 0;
		if (ps->p[0] == '[' && ps->p[1] == ':') {
			const char *name = ps->p + 2, *end = name;
			while (*end != '\0' && !(end[0] == ':' && end[1] == ']'))
				end++;
			if (*end == '\0') {
				re->error = 1;
				return f;
			}
			for (unsigned c = 0; c < 256; c++)
				if (is_in_named_class(name, (size_t)(end - name), c))
					class_set(cls, c);
			ps->p = end + 2;
			continue;
		}
		unsigned lo = (unsigned char)*ps->p++;
		if (lo == '\\' && *ps->p != '\0')
			lo = (unsigned char)*ps->p++;
		unsigned hi = lo;
		if (ps->p[0] == '-' && ps->p[1] != ']' && ps->p[1] != '\0') {
			hi = (unsigned char)ps->p[1];
			ps->p += 2;
		}
		for (unsigned c = lo; c <= hi && c < 256; c++)
			class_set(cls, c);
	}
	if (*ps->p != ']') {
		re->error = 1;
		return f;
	}
	ps->p++;
	if (negate)
		for (int i = 0; i < 32; i++)
			cls[i] = (uint8_t)~cls[i];
	return f;
}

static struct frag parse_alt(struct parser *ps, int depth);

static struct frag parse_atom(struct parser *ps, int depth) {
	struct regex *re = ps->re;
	char c = *ps->p;
	if (c == '(') {
		ps->p++;
		struct frag f = parse_alt(ps, depth + 1);
		if (*ps->p != ')')
			re->error = 1;
		else
			ps->p++;
		return f;
	}
	ps->p++;
	if (c == '.')
		return single(re, ST_ANY, 0);
	if (c == '[')
		return parse_class(ps);
	if (c == '^')
		return single(re, ST_BOL, 0);
	if (c == '$')
		return single(re, ST_EOL, 0);
	if (c == '\\') {
		c = *ps->p;
		if (c == '\0') {
			re->error = 1;
			return single(re, ST_CHAR, 0);
		}
		ps->p++;
	}
	return single(re, ST_CHAR, (unsigned char)c);
}

/* A copy of a fragment whose states are [start, end) (for counted
   repetition): targets and dangling links inside the range move. */
static struct frag copy_frag(struct regex *re, int start, int end, struct frag f) {
	int base = re->nstates;
	struct frag g = f;
	for (int i = start; i < end; i++) {
		int n = new_state(re, re->states[i].type, re->states[i].c);
		if (re->error)
			return f;
		struct state *s = &re->states[n];
		s->cls = re->states[i].cls;
		int16_t o = re->states[i].out, o1 = re->states[i].out1;
		s->out = (int16_t)(o >= start && o < end ? o - start + base : o);
		s->out1 = (int16_t)(o1 >= start && o1 < end ? o1 - start + base : o1);
		for (int w = 0; w < 2; w++) {
			int16_t l = re->links[i * 2 + w];
			re->links[n * 2 + w] = (int16_t)(l >= start * 2 && l < end * 2 ? l - start * 2 + base * 2 : l);
		}
	}
	g.start = f.start - start + base;
	g.first = f.first - start * 2 + base * 2;
	g.last = f.last - start * 2 + base * 2;
	return g;
}

static struct frag star(struct regex *re, struct frag f) {
	int s = new_state(re, ST_SPLIT, 0);
	re->states[s].out = (int16_t)f.start;
	patch(re, f.first, s);
	struct frag g;
	g.start = s;
	g.first = s * 2 + 1;
	g.last = g.first;
	re->links[g.first] = -1;
	return g;
}

static struct frag optional(struct regex *re, struct frag f) {
	int s = new_state(re, ST_SPLIT, 0);
	re->states[s].out = (int16_t)f.start;
	struct frag g;
	g.start = s;
	re->links[s * 2 + 1] = -1;
	g.first = append(re, f.first, s * 2 + 1);
	g.last = g.first;
	return g;
}

static void concat(struct regex *re, struct frag *a, struct frag b) {
	if (a->start < 0) {
		*a = b;
		return;
	}
	patch(re, a->first, b.start);
	a->first = b.first;
	a->last = b.last;
}

static int parse_number(struct parser *ps) {
	int n = 0, digits = 0;
	while (*ps->p >= '0' && *ps->p <= '9' && digits < 4) {
		n = n * 10 + (*ps->p++ - '0');
		digits++;
	}
	return digits ? n : -1;
}

static struct frag parse_piece(struct parser *ps, int depth) {
	struct regex *re = ps->re;
	int start = re->nstates;
	struct frag f = parse_atom(ps, depth);
	int end = re->nstates;
	for (;;) {
		char c = *ps->p;
		if (c == '*') {
			ps->p++;
			f = star(re, f);
		} else if (c == '+') {
			ps->p++;
			struct frag loop = copy_frag(re, start, end, f);
			struct frag s = star(re, loop);
			concat(re, &f, s);
		} else if (c == '?') {
			ps->p++;
			f = optional(re, f);
		} else if (c == '{') {
			ps->p++;
			int m = parse_number(ps), n = m;
			if (*ps->p == ',') {
				ps->p++;
				n = (*ps->p == '}') ? -1 : parse_number(ps);
			}
			if (*ps->p != '}' || m < 0 || m > 16 || (n != -1 && (n < m || n > 16))) {
				re->error = 1;
				return f;
			}
			ps->p++;
			struct frag result;
			result.start = -1;
			result.first = result.last = -1;
			for (int i = 0; i < m; i++)
				concat(re, &result, i == 0 ? f : copy_frag(re, start, end, f));
			if (n == -1) {
				struct frag s = star(re, m == 0 ? f : copy_frag(re, start, end, f));
				concat(re, &result, s);
			} else {
				for (int i = m; i < n; i++) {
					struct frag o = optional(re, (i == 0 && m == 0) ? f : copy_frag(re, start, end, f));
					concat(re, &result, o);
				}
			}
			if (result.start < 0) {
				/* x{0}: matches nothing: an empty split. */
				int s = new_state(re, ST_SPLIT, 0);
				result.start = s;
				result.first = s * 2;
				result.last = result.first;
			}
			f = result;
		} else
			break;
		if (re->error)
			break;
	}
	return f;
}

static struct frag parse_concat(struct parser *ps, int depth) {
	struct frag f;
	f.start = -1;
	f.first = f.last = -1;
	while (*ps->p != '\0' && *ps->p != '|' && *ps->p != ')' && !ps->re->error)
		concat(ps->re, &f, parse_piece(ps, depth));
	if (f.start < 0) {
		int s = new_state(ps->re, ST_SPLIT, 0);
		f.start = s;
		f.first = s * 2;
		f.last = f.first;
	}
	return f;
}

static struct frag parse_alt(struct parser *ps, int depth) {
	struct regex *re = ps->re;
	if (depth > 32) {
		re->error = 1;
		return single(re, ST_CHAR, 0);
	}
	struct frag f = parse_concat(ps, depth);
	while (*ps->p == '|' && !re->error) {
		ps->p++;
		struct frag g = parse_concat(ps, depth);
		int s = new_state(re, ST_SPLIT, 0);
		re->states[s].out = (int16_t)f.start;
		re->states[s].out1 = (int16_t)g.start;
		f.start = s;
		f.first = append(re, f.first, g.first);
	}
	return f;
}

/* ---- Running ---- */

struct list {
	int16_t s[MAX_STATES];
	int n;
};

static void add(const struct regex *re, struct list *l, uint8_t *mark, int gen, int s, int atStart, int atEnd) {
	/* An explicit stack instead of recursion through splits. */
	int16_t stack[MAX_STATES];
	int sp = 0;
	stack[sp++] = (int16_t)s;
	while (sp > 0) {
		int i = stack[--sp];
		if (i < 0 || mark[i] == gen)
			continue;
		mark[i] = (uint8_t)gen;
		const struct state *st = &re->states[i];
		if (st->type == ST_SPLIT) {
			if (sp + 2 <= MAX_STATES) {
				stack[sp++] = st->out1;
				stack[sp++] = st->out;
			}
		} else if (st->type == ST_BOL) {
			if (atStart)
				stack[sp++] = st->out;
		} else if (st->type == ST_EOL) {
			if (atEnd)
				stack[sp++] = st->out;
		} else
			l->s[l->n++] = (int16_t)i;
	}
}

struct sb_regex_workspace {
	struct regex re;
	struct list cur, next;
	uint8_t mark[MAX_STATES];
};

size_t sb_regex_workspace_size(void) {
	return sizeof(struct sb_regex_workspace);
}

/* The workspace is sb_regex_workspace_size() bytes: the kernel's comes
   from its heap, userland's is on the stack (sb_regex_search). */
int sb_regex_search_ws(const char *pattern, const char *subject, void *workspace) {
	struct sb_regex_workspace *ws = (struct sb_regex_workspace *)workspace;
	struct regex *rx = &ws->re;
	struct list *curp = &ws->cur, *nextp = &ws->next;
	uint8_t *mark = ws->mark;
	rx->nstates = 0;
	rx->nclasses = 0;
	rx->error = 0;
	struct parser ps;
	ps.re = rx;
	ps.p = pattern;
	struct frag f = parse_alt(&ps, 0);
	if (*ps.p != '\0')
		rx->error = 1;
	int match = new_state(rx, ST_MATCH, 0);
	if (rx->error)
		return -1;
	patch(rx, f.first, match);

	for (int i = 0; i < MAX_STATES; i++)
		mark[i] = 0;
	int gen = 1;
	size_t len = 0;
	while (subject[len] != '\0')
		len++;
	curp->n = 0;
	add(rx, curp, mark, gen, f.start, 1, len == 0);
	for (size_t pos = 0;; pos++) {
		for (int i = 0; i < curp->n; i++)
			if (rx->states[curp->s[i]].type == ST_MATCH)
				return 1;
		if (pos == len)
			return 0;
		unsigned c = (unsigned char)subject[pos];
		gen = gen == 255 ? 1 : gen + 1;
		if (gen == 1)
			for (int i = 0; i < MAX_STATES; i++)
				mark[i] = 0;
		nextp->n = 0;
		for (int i = 0; i < curp->n; i++) {
			const struct state *st = &rx->states[curp->s[i]];
			int ok = (st->type == ST_ANY && c != '\n') || (st->type == ST_CHAR && st->c == c) ||
			    (st->type == ST_CLASS && class_bit(rx->classes[st->cls], c));
			if (ok)
				add(rx, nextp, mark, gen, st->out, 0, pos + 1 == len);
		}
		/* A search: a match may also start at the next position. */
		add(rx, nextp, mark, gen, f.start, 0, pos + 1 == len);
		{ struct list *t = curp; curp = nextp; nextp = t; }
	}
}

#ifndef KERNEL
int sb_regex_search(const char *pattern, const char *subject) {
	struct sb_regex_workspace ws;
	return sb_regex_search_ws(pattern, subject, &ws);
}
#endif
