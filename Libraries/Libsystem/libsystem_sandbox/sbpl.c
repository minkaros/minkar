/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Sandbox profiles (SBPL): a small Scheme read and run to collect rules,
 * then compiled to the blob the kernel enforces (sbblob.h).
 *
 * The language: strings, numbers (#o, #x), #t/#f, regex literals #"...",
 * quote, define (values and functions), lambda, let, if, when, unless,
 * cond, begin, and, or, not, comparisons, string functions, and the
 * profile's own forms: version, param, allow, deny, with, with-filter,
 * debug, and the filters. A symbol nothing defines stands for itself
 * (an operation such as file-read*, or a constant such as DIRECTORY).
 */

#include "sbpl.h"
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---- Memory: everything of one compilation in one arena ---- */

struct block {
	struct block *next;
	size_t used, size;
	char bytes[];
};

struct arena {
	struct block *blocks;
};

static void *arena_alloc(struct arena *a, size_t n) {
	n = (n + 15) & ~(size_t)15;
	if (a->blocks == NULL || a->blocks->size - a->blocks->used < n) {
		size_t size = n > 65536 ? n : 65536;
		struct block *b = malloc(sizeof(struct block) + size);
		if (b == NULL)
			return NULL;
		b->next = a->blocks;
		b->used = 0;
		b->size = size;
		a->blocks = b;
	}
	void *p = a->blocks->bytes + a->blocks->used;
	a->blocks->used += n;
	memset(p, 0, n);
	return p;
}

static void arena_free(struct arena *a) {
	while (a->blocks != NULL) {
		struct block *next = a->blocks->next;
		free(a->blocks);
		a->blocks = next;
	}
}

/* ---- Values ---- */

enum vtype { V_FALSE, V_TRUE, V_NUM, V_STR, V_SYM, V_REGEX, V_PAIR, V_FILTER, V_MOD, V_CLOSURE, V_BUILTIN, V_NIL };

struct filter;
struct env;
struct interp;
typedef struct value value;
typedef value *(*builtin_fn)(struct interp *, value *args, struct env *);

struct value {
	int type;
	double num;
	const char *s;		/* string, symbol, regex, modifier name */
	value *car, *cdr;
	struct filter *filter;
	value *params, *body;	/* closure */
	struct env *env;
	builtin_fn fn;
	int special;		/* a form that gets its arguments unevaluated */
	int modflags;
};

struct filter {
	int type;		/* SBF_* */
	int kind, match;
	const char *str;
	uint32_t value;
	int n;
	struct filter **kids;
};

struct binding {
	struct binding *next;
	const char *name;
	value *v;
};

struct env {
	struct env *parent;
	struct binding *bindings;
};

struct rule {
	const char *operation;
	int action, flags;
	struct filter *filter;
};

struct interp {
	struct arena arena;
	const char *const *params;
	struct rule *rules;
	size_t nrules, rulecap;
	int default_action, default_flags, has_default;
	struct filter *context[16];	/* with-filter */
	int ncontext;
	char error[256];
	int failed;
	int depth;
	value *current;		/* the builtin being applied */
	value nil, vtrue, vfalse;
};

static value *mk(struct interp *in, int type) {
	value *v = arena_alloc(&in->arena, sizeof(value));
	if (v == NULL) {
		in->failed = 1;
		snprintf(in->error, sizeof(in->error), "out of memory");
		return &in->nil;
	}
	v->type = type;
	return v;
}

static const char *dup_n(struct interp *in, const char *s, size_t n) {
	char *d = arena_alloc(&in->arena, n + 1);
	if (d == NULL)
		return "";
	memcpy(d, s, n);
	d[n] = '\0';
	return d;
}

static value *mkstr(struct interp *in, int type, const char *s, size_t n) {
	value *v = mk(in, type);
	v->s = dup_n(in, s, n);
	return v;
}

static value *cons(struct interp *in, value *a, value *b) {
	value *v = mk(in, V_PAIR);
	v->car = a;
	v->cdr = b;
	return v;
}

static int fail(struct interp *in, const char *fmt, ...) {
	if (!in->failed) {
		va_list ap;
		va_start(ap, fmt);
		vsnprintf(in->error, sizeof(in->error), fmt, ap);
		va_end(ap);
		in->failed = 1;
	}
	return 0;
}

static int truthy(value *v) {
	return v->type != V_FALSE;
}

/* ---- Reading ---- */

struct reader {
	const char *p;
	int line;
};

static void skip_space(struct reader *r) {
	for (;;) {
		while (*r->p != '\0' && isspace((unsigned char)*r->p)) {
			if (*r->p == '\n')
				r->line++;
			r->p++;
		}
		if (*r->p == ';') {
			while (*r->p != '\0' && *r->p != '\n')
				r->p++;
		} else if (r->p[0] == '#' && r->p[1] == '|') {
			r->p += 2;
			while (*r->p != '\0' && !(r->p[0] == '|' && r->p[1] == '#')) {
				if (*r->p == '\n')
					r->line++;
				r->p++;
			}
			if (*r->p != '\0')
				r->p += 2;
		} else
			return;
	}
}

static value *read_form(struct interp *in, struct reader *r);

static value *read_string(struct interp *in, struct reader *r, int type) {
	r->p++;	/* the opening quote */
	size_t cap = 64, n = 0;
	char *buf = malloc(cap);
	if (buf == NULL) {
		fail(in, "out of memory");
		return &in->nil;
	}
	while (*r->p != '\0' && *r->p != '"') {
		char c = *r->p++;
		if (c == '\\' && *r->p != '\0') {
			char e = *r->p++;
			/* A regex literal keeps its escapes for the regex. */
			if (type == V_REGEX) {
				if (n + 2 >= cap)
					buf = realloc(buf, cap *= 2);
				buf[n++] = '\\';
				c = e;
			} else
				c = e == 'n' ? '\n' : e == 't' ? '\t' : e;
		} else if (c == '\n')
			r->line++;
		if (n + 2 >= cap)
			buf = realloc(buf, cap *= 2);
		if (buf == NULL) {
			fail(in, "out of memory");
			return &in->nil;
		}
		buf[n++] = c;
	}
	if (*r->p != '"') {
		free(buf);
		fail(in, "line %d: unterminated string", r->line);
		return &in->nil;
	}
	r->p++;
	value *v = mkstr(in, type, buf, n);
	free(buf);
	return v;
}

static int delimiter(char c) {
	return c == '\0' || isspace((unsigned char)c) || c == '(' || c == ')' || c == '"' || c == ';' || c == '\'';
}

static value *read_atom(struct interp *in, struct reader *r) {
	const char *start = r->p;
	while (!delimiter(*r->p))
		r->p++;
	size_t n = (size_t)(r->p - start);
	if (n == 2 && start[0] == '#' && (start[1] == 't' || start[1] == 'f'))
		return start[1] == 't' ? &in->vtrue : &in->vfalse;
	if (n > 2 && start[0] == '#' && (start[1] == 'o' || start[1] == 'x' || start[1] == 'b' || start[1] == 'd')) {
		char tmp[64];
		snprintf(tmp, sizeof(tmp), "%.*s", (int)(n - 2), start + 2);
		value *v = mk(in, V_NUM);
		v->num = (double)strtol(tmp, NULL, start[1] == 'o' ? 8 : start[1] == 'x' ? 16 : start[1] == 'b' ? 2 : 10);
		return v;
	}
	if (isdigit((unsigned char)start[0]) || ((start[0] == '-' || start[0] == '+') && n > 1 && isdigit((unsigned char)start[1]))) {
		char tmp[64], *end;
		snprintf(tmp, sizeof(tmp), "%.*s", (int)n, start);
		double d = strtod(tmp, &end);
		if (*end == '\0') {
			value *v = mk(in, V_NUM);
			v->num = d;
			return v;
		}
	}
	return mkstr(in, V_SYM, start, n);
}

static value *read_form(struct interp *in, struct reader *r) {
	if (++in->depth > 200) {
		fail(in, "line %d: nested too deeply", r->line);
		return &in->nil;
	}
	skip_space(r);
	value *result;
	char c = *r->p;
	if (c == '\0') {
		fail(in, "line %d: unexpected end", r->line);
		result = &in->nil;
	} else if (c == '(') {
		r->p++;
		value *head = &in->nil, **tail = &head;
		for (;;) {
			skip_space(r);
			if (*r->p == ')') {
				r->p++;
				break;
			}
			if (*r->p == '\0' || in->failed) {
				fail(in, "line %d: unbalanced parentheses", r->line);
				break;
			}
			value *item = read_form(in, r);
			*tail = cons(in, item, &in->nil);
			tail = &(*tail)->cdr;
		}
		result = head;
	} else if (c == ')') {
		r->p++;
		fail(in, "line %d: unexpected ')'", r->line);
		result = &in->nil;
	} else if (c == '\'') {
		r->p++;
		value *q = read_form(in, r);
		result = cons(in, mkstr(in, V_SYM, "quote", 5), cons(in, q, &in->nil));
	} else if (c == '"')
		result = read_string(in, r, V_STR);
	else if (c == '#' && r->p[1] == '"') {
		r->p++;
		result = read_string(in, r, V_REGEX);
	} else
		result = read_atom(in, r);
	in->depth--;
	return result;
}

/* ---- Environments ---- */

static value *lookup(struct env *env, const char *name) {
	for (; env != NULL; env = env->parent)
		for (struct binding *b = env->bindings; b != NULL; b = b->next)
			if (strcmp(b->name, name) == 0)
				return b->v;
	return NULL;
}

static void bind(struct interp *in, struct env *env, const char *name, value *v) {
	for (struct binding *b = env->bindings; b != NULL; b = b->next)
		if (strcmp(b->name, name) == 0) {
			b->v = v;
			return;
		}
	struct binding *b = arena_alloc(&in->arena, sizeof(*b));
	if (b == NULL) {
		fail(in, "out of memory");
		return;
	}
	b->name = name;
	b->v = v;
	b->next = env->bindings;
	env->bindings = b;
}

static struct env *new_env(struct interp *in, struct env *parent) {
	struct env *e = arena_alloc(&in->arena, sizeof(*e));
	if (e == NULL) {
		fail(in, "out of memory");
		return parent;
	}
	e->parent = parent;
	return e;
}

/* ---- Evaluation ---- */

static value *eval(struct interp *in, value *v, struct env *env);

static value *eval_body(struct interp *in, value *body, struct env *env) {
	value *result = &in->nil;
	for (; body->type == V_PAIR && !in->failed; body = body->cdr)
		result = eval(in, body->car, env);
	return result;
}

static value *apply(struct interp *in, value *f, value *args, struct env *env) {
	if (f->type == V_BUILTIN) {
		in->current = f;
		return f->fn(in, args, env);
	}
	if (f->type != V_CLOSURE) {
		fail(in, "%s is not a function", f->type == V_SYM ? f->s : "a value");
		return &in->nil;
	}
	struct env *frame = new_env(in, f->env);
	value *p = f->params, *a = args;
	for (; p->type == V_PAIR; p = p->cdr) {
		bind(in, frame, p->car->s, a->type == V_PAIR ? a->car : &in->vfalse);
		if (a->type == V_PAIR)
			a = a->cdr;
	}
	if (p->type == V_SYM)	/* a rest parameter: (lambda args ...) */
		bind(in, frame, p->s, a);
	return eval_body(in, f->body, frame);
}

static value *eval(struct interp *in, value *v, struct env *env) {
	if (in->failed)
		return &in->nil;
	switch (v->type) {
	case V_SYM: {
		value *bound = lookup(env, v->s);
		return bound != NULL ? bound : v;
	}
	case V_PAIR: {
		if (++in->depth > 400) {
			fail(in, "recursion too deep");
			return &in->nil;
		}
		value *f = eval(in, v->car, env);
		value *result;
		if (f->type == V_BUILTIN && f->special)
			result = f->fn(in, v->cdr, env);
		else {
			value *head = &in->nil, **tail = &head;
			for (value *a = v->cdr; a->type == V_PAIR && !in->failed; a = a->cdr) {
				*tail = cons(in, eval(in, a->car, env), &in->nil);
				tail = &(*tail)->cdr;
			}
			result = apply(in, f, head, env);
		}
		in->depth--;
		return result;
	}
	default:
		return v;
	}
}

static value *arg(value *args, int i) {
	for (; i > 0 && args->type == V_PAIR; i--)
		args = args->cdr;
	return args->type == V_PAIR ? args->car : NULL;
}

static value *boolean(struct interp *in, int b) {
	return b ? &in->vtrue : &in->vfalse;
}

/* -- Special forms -- */

static value *sf_quote(struct interp *in, value *args, struct env *env) {
	(void)env;
	return args->type == V_PAIR ? args->car : &in->nil;
}

static value *sf_define(struct interp *in, value *args, struct env *env) {
	value *target = arg(args, 0);
	if (target == NULL)
		return &in->nil;
	if (target->type == V_PAIR) {	/* (define (f params...) body...) */
		value *f = mk(in, V_CLOSURE);
		f->params = target->cdr;
		f->body = args->cdr;
		f->env = env;
		if (target->car->type == V_SYM)
			bind(in, env, target->car->s, f);
	} else if (target->type == V_SYM) {
		value *x = arg(args, 1);
		bind(in, env, target->s, x != NULL ? eval(in, x, env) : &in->vfalse);
	}
	return &in->nil;
}

static value *sf_lambda(struct interp *in, value *args, struct env *env) {
	value *f = mk(in, V_CLOSURE);
	f->params = args->type == V_PAIR ? args->car : &in->nil;
	f->body = args->type == V_PAIR ? args->cdr : &in->nil;
	f->env = env;
	return f;
}

static value *sf_if(struct interp *in, value *args, struct env *env) {
	value *c = arg(args, 0);
	if (c == NULL)
		return &in->nil;
	if (truthy(eval(in, c, env)))
		return arg(args, 1) ? eval(in, arg(args, 1), env) : &in->nil;
	return arg(args, 2) ? eval(in, arg(args, 2), env) : &in->nil;
}

static value *sf_when(struct interp *in, value *args, struct env *env) {
	if (args->type == V_PAIR && truthy(eval(in, args->car, env)))
		return eval_body(in, args->cdr, env);
	return &in->nil;
}

static value *sf_unless(struct interp *in, value *args, struct env *env) {
	if (args->type == V_PAIR && !truthy(eval(in, args->car, env)))
		return eval_body(in, args->cdr, env);
	return &in->nil;
}

static value *sf_begin(struct interp *in, value *args, struct env *env) {
	return eval_body(in, args, env);
}

static value *sf_let(struct interp *in, value *args, struct env *env) {
	struct env *frame = new_env(in, env);
	value *b = arg(args, 0);
	for (; b != NULL && b->type == V_PAIR; b = b->cdr) {
		value *pair = b->car;
		if (pair->type == V_PAIR && pair->car->type == V_SYM)
			bind(in, frame, pair->car->s, arg(pair, 1) ? eval(in, arg(pair, 1), env) : &in->vfalse);
	}
	return eval_body(in, args->type == V_PAIR ? args->cdr : &in->nil, frame);
}

static value *sf_cond(struct interp *in, value *args, struct env *env) {
	for (; args->type == V_PAIR; args = args->cdr) {
		value *clause = args->car;
		if (clause->type != V_PAIR)
			continue;
		if ((clause->car->type == V_SYM && strcmp(clause->car->s, "else") == 0) || truthy(eval(in, clause->car, env)))
			return eval_body(in, clause->cdr, env);
	}
	return &in->nil;
}

static value *sf_and(struct interp *in, value *args, struct env *env) {
	value *result = &in->vtrue;
	for (; args->type == V_PAIR; args = args->cdr)
		if (!truthy(result = eval(in, args->car, env)))
			return result;
	return result;
}

static value *sf_or(struct interp *in, value *args, struct env *env) {
	for (; args->type == V_PAIR; args = args->cdr) {
		value *result = eval(in, args->car, env);
		if (truthy(result))
			return result;
	}
	return &in->vfalse;
}

static value *sf_defined(struct interp *in, value *args, struct env *env) {
	value *a = arg(args, 0);
	if (a != NULL && a->type != V_SYM)
		a = eval(in, a, env);	/* (defined? 'name) */
	return boolean(in, a != NULL && a->type == V_SYM && lookup(env, a->s) != NULL);
}

/* -- Plain functions -- */

static value *fn_version(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *v = arg(args, 0);
	if (v == NULL || v->type != V_NUM || v->num != 1)
		fail(in, "only (version 1) profiles are known");
	return &in->nil;
}

static value *fn_param(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *name = arg(args, 0);
	if (name == NULL || name->type != V_STR || in->params == NULL)
		return &in->vfalse;
	for (const char *const *p = in->params; p[0] != NULL && p[1] != NULL; p += 2)
		if (strcmp(p[0], name->s) == 0)
			return mkstr(in, V_STR, p[1], strlen(p[1]));
	return &in->vfalse;
}

static value *fn_string_append(struct interp *in, value *args, struct env *env) {
	(void)env;
	size_t n = 0;
	for (value *a = args; a->type == V_PAIR; a = a->cdr)
		if (a->car->type == V_STR || a->car->type == V_REGEX || a->car->type == V_SYM)
			n += strlen(a->car->s);
	char *buf = arena_alloc(&in->arena, n + 1);
	if (buf == NULL)
		return &in->nil;
	char *p = buf;
	int regex = 0;
	for (value *a = args; a->type == V_PAIR; a = a->cdr)
		if (a->car->type == V_STR || a->car->type == V_REGEX || a->car->type == V_SYM) {
			size_t l = strlen(a->car->s);
			memcpy(p, a->car->s, l);
			p += l;
			regex |= a->car->type == V_REGEX;
		}
	*p = '\0';
	value *v = mk(in, regex ? V_REGEX : V_STR);
	v->s = buf;
	return v;
}

static int strings2(value *args, const char **a, const char **b) {
	value *x = arg(args, 0), *y = arg(args, 1);
	if (x == NULL || y == NULL || x->type != V_STR || y->type != V_STR)
		return 0;
	*a = x->s;
	*b = y->s;
	return 1;
}

static value *fn_string_eq(struct interp *in, value *args, struct env *env) {
	(void)env;
	const char *a, *b;
	return boolean(in, strings2(args, &a, &b) && strcmp(a, b) == 0);
}

static value *fn_string_prefix(struct interp *in, value *args, struct env *env) {
	(void)env;
	const char *a, *b;
	return boolean(in, strings2(args, &a, &b) && strncmp(a, b, strlen(a)) == 0);
}

static value *fn_stringp(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *v = arg(args, 0);
	return boolean(in, v != NULL && v->type == V_STR);
}

static value *fn_string_to_number(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *v = arg(args, 0);
	if (v == NULL || v->type != V_STR)
		return &in->vfalse;
	char *end;
	double d = strtod(v->s, &end);
	if (end == v->s)
		return &in->vfalse;
	value *n = mk(in, V_NUM);
	n->num = d;
	return n;
}

static value *fn_number_to_string(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *v = arg(args, 0);
	char tmp[64];
	snprintf(tmp, sizeof(tmp), "%g", v != NULL && v->type == V_NUM ? v->num : 0.0);
	return mkstr(in, V_STR, tmp, strlen(tmp));
}

static value *compare(struct interp *in, value *args, int op) {
	value *a = arg(args, 0), *b = arg(args, 1);
	if (a == NULL || b == NULL || a->type != V_NUM || b->type != V_NUM)
		return &in->vfalse;
	switch (op) {
	case 0: return boolean(in, a->num == b->num);
	case 1: return boolean(in, a->num < b->num);
	case 2: return boolean(in, a->num > b->num);
	case 3: return boolean(in, a->num <= b->num);
	default: return boolean(in, a->num >= b->num);
	}
}

static value *fn_num_eq(struct interp *in, value *a, struct env *e) { (void)e; return compare(in, a, 0); }
static value *fn_lt(struct interp *in, value *a, struct env *e) { (void)e; return compare(in, a, 1); }
static value *fn_gt(struct interp *in, value *a, struct env *e) { (void)e; return compare(in, a, 2); }
static value *fn_le(struct interp *in, value *a, struct env *e) { (void)e; return compare(in, a, 3); }
static value *fn_ge(struct interp *in, value *a, struct env *e) { (void)e; return compare(in, a, 4); }

static int equal_values(value *a, value *b) {
	if (a->type != b->type)
		return 0;
	if (a->type == V_NUM)
		return a->num == b->num;
	if (a->type == V_STR || a->type == V_SYM || a->type == V_REGEX)
		return strcmp(a->s, b->s) == 0;
	return a == b || a->type == V_TRUE || a->type == V_FALSE || a->type == V_NIL;
}

static value *fn_equal(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *a = arg(args, 0), *b = arg(args, 1);
	return boolean(in, a != NULL && b != NULL && equal_values(a, b));
}

static value *fn_not(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *a = arg(args, 0);
	return boolean(in, a == NULL || !truthy(a));
}

static value *fn_list(struct interp *in, value *args, struct env *env) {
	(void)in;
	(void)env;
	return args;
}

static value *fn_ignore(struct interp *in, value *args, struct env *env) {
	(void)args;
	(void)env;
	return &in->nil;
}

/* A string with the regex metacharacters escaped. */
static value *fn_regex_quote(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *v = arg(args, 0);
	if (v == NULL || v->type != V_STR)
		return &in->vfalse;
	size_t n = strlen(v->s);
	char *buf = arena_alloc(&in->arena, n * 2 + 1), *p = buf;
	if (buf == NULL)
		return &in->nil;
	for (const char *s = v->s; *s != '\0'; s++) {
		if (strchr(".[]()*+?{}|^$\\", *s) != NULL)
			*p++ = '\\';
		*p++ = *s;
	}
	*p = '\0';
	value *r = mk(in, V_STR);
	r->s = buf;
	return r;
}

/* -- Filters -- */

static struct filter *new_filter(struct interp *in, int type) {
	struct filter *f = arena_alloc(&in->arena, sizeof(*f));
	if (f == NULL) {
		fail(in, "out of memory");
		static struct filter never = { SBF_NEVER, 0, 0, NULL, 0, 0, NULL };
		return &never;
	}
	f->type = type;
	return f;
}

static value *filter_value(struct interp *in, struct filter *f) {
	value *v = mk(in, V_FILTER);
	v->filter = f;
	return v;
}

/* Several alternatives as one filter (one is itself). */
static struct filter *any_of(struct interp *in, struct filter **kids, int n, int type) {
	if (n == 1)
		return kids[0];
	struct filter *f = new_filter(in, type);
	f->n = n;
	f->kids = arena_alloc(&in->arena, sizeof(struct filter *) * (size_t)(n > 0 ? n : 1));
	if (f->kids == NULL) {
		f->n = 0;
		return f;
	}
	memcpy(f->kids, kids, sizeof(struct filter *) * (size_t)n);
	return f;
}

/* A string filter: each argument a string (literal by default) or a
   regex; several are alternatives. */
static value *string_filter(struct interp *in, value *args, int type, int kind, int match) {
	struct filter *kids[64];
	int n = 0;
	for (; args->type == V_PAIR && n < 64; args = args->cdr) {
		value *a = args->car;
		struct filter *f;
		if (a->type == V_STR || a->type == V_REGEX) {
			f = new_filter(in, type);
			f->kind = kind;
			f->match = a->type == V_REGEX ? SBM_REGEX : match;
			f->str = a->s;
		} else if (a->type == V_FILTER && a->filter->type == SBF_PATH && type == SBF_NAME) {
			/* (global-name (regex ...)): the inner filter's pattern. */
			f = new_filter(in, type);
			f->kind = kind;
			f->match = a->filter->match;
			f->str = a->filter->str;
		} else
			continue;
		kids[n++] = f;
	}
	if (n == 0) {
		fail(in, "%s: a filter needs a string", in->current != NULL && in->current->s != NULL ? in->current->s : "filter");
		return filter_value(in, new_filter(in, SBF_NEVER));
	}
	return filter_value(in, any_of(in, kids, n, SBF_ANY));
}

#define PATH_FILTER(fn, match) \
	static value *fn(struct interp *in, value *args, struct env *env) { \
		(void)env; return string_filter(in, args, SBF_PATH, 0, match); }
PATH_FILTER(f_literal, SBM_LITERAL)
PATH_FILTER(f_subpath, SBM_SUBPATH)
PATH_FILTER(f_prefix, SBM_PREFIX)
PATH_FILTER(f_regex, SBM_REGEX)

static const struct {
	const char *name;
	int kind;
} name_filters[] = {
	{ "global-name", SBN_GLOBAL_NAME }, { "local-name", SBN_LOCAL_NAME },
	{ "sysctl-name", SBN_SYSCTL_NAME }, { "ipc-posix-name", SBN_IPC_POSIX_NAME },
	{ "iokit-user-client-class", SBN_IOKIT_USER_CLIENT_CLASS }, { "iokit-property", SBN_IOKIT_PROPERTY },
	{ "iokit-registry-entry-class", SBN_IOKIT_REGISTRY_ENTRY_CLASS }, { "iokit-connection", SBN_IOKIT_CONNECTION },
	{ "preference-domain", SBN_PREFERENCE_DOMAIN }, { "xpc-service-name", SBN_XPC_SERVICE_NAME },
	{ "control-name", SBN_CONTROL_NAME }, { "extension", SBN_EXTENSION },
	{ "extension-class", SBN_EXTENSION_CLASS }, { "signal-name", SBN_SIGNAL_NAME },
};

static const struct {
	const char *name;
	int value;
} constants[] = {
	{ "REGULAR-FILE", SBV_REGULAR_FILE }, { "DIRECTORY", SBV_DIRECTORY }, { "SYMLINK", SBV_SYMLINK },
	{ "CHARACTER-DEVICE", SBV_CHARACTER_DEVICE }, { "BLOCK-DEVICE", SBV_BLOCK_DEVICE },
	{ "FIFO", SBV_FIFO }, { "SOCKET", SBV_SOCKET }, { "TTY", SBV_CHARACTER_DEVICE },
	{ "self", SBT_SELF }, { "others", SBT_OTHERS }, { "children", SBT_CHILDREN }, { "pgrp", SBT_PGRP },
	{ "same-sandbox", SBT_SAME_SANDBOX },
	{ "AF_UNIX", 1 }, { "AF_LOCAL", 1 }, { "AF_INET", 2 }, { "AF_ROUTE", 17 }, { "AF_INET6", 30 },
	{ "AF_SYSTEM", 32 }, { "SOCK_STREAM", 1 }, { "SOCK_DGRAM", 2 }, { "SOCK_RAW", 3 },
	{ "SYSPROTO_EVENT", 1 }, { "SYSPROTO_CONTROL", 2 },
};

static int constant_value(value *v, uint32_t *out) {
	if (v->type == V_NUM) {
		*out = (uint32_t)v->num;
		return 1;
	}
	if (v->type != V_SYM)
		return 0;
	for (size_t i = 0; i < sizeof(constants) / sizeof(constants[0]); i++)
		if (strcmp(constants[i].name, v->s) == 0) {
			*out = (uint32_t)constants[i].value;
			return 1;
		}
	return 0;
}

static value *int_filter(struct interp *in, value *args, int kind) {
	struct filter *kids[16];
	int n = 0;
	for (; args->type == V_PAIR && n < 16; args = args->cdr) {
		uint32_t v;
		if (!constant_value(args->car, &v))
			continue;
		struct filter *f = new_filter(in, SBF_INT);
		f->kind = kind;
		f->value = v;
		kids[n++] = f;
	}
	if (n == 0)
		return filter_value(in, new_filter(in, SBF_NEVER));
	return filter_value(in, any_of(in, kids, n, SBF_ANY));
}

static value *f_vnode_type(struct interp *in, value *a, struct env *e) { (void)e; return int_filter(in, a, SBI_VNODE_TYPE); }
static value *f_target(struct interp *in, value *a, struct env *e) { (void)e; return int_filter(in, a, SBI_TARGET); }
static value *f_socket_domain(struct interp *in, value *a, struct env *e) { (void)e; return int_filter(in, a, SBI_SOCKET_DOMAIN); }
static value *f_socket_type(struct interp *in, value *a, struct env *e) { (void)e; return int_filter(in, a, SBI_SOCKET_TYPE); }
static value *f_socket_protocol(struct interp *in, value *a, struct env *e) { (void)e; return int_filter(in, a, SBI_SOCKET_PROTOCOL); }
static value *f_file_mode(struct interp *in, value *a, struct env *e) { (void)e; return int_filter(in, a, SBI_FILE_MODE); }

/* (local ip "host:port"), (remote tcp "*:443"), (remote unix-socket (path-literal "/p")). */
static value *address_filter(struct interp *in, value *args, int kind) {
	value *proto = arg(args, 0), *where = arg(args, 1);
	if (proto == NULL || proto->type != V_SYM)
		return filter_value(in, new_filter(in, SBF_NEVER));
	char buf[512];
	if (strcmp(proto->s, "unix") == 0 || strcmp(proto->s, "unix-socket") == 0) {
		const char *path = "*";
		if (where != NULL && where->type == V_STR)
			path = where->s;
		else if (where != NULL && where->type == V_FILTER && where->filter->str != NULL)
			path = where->filter->str;
		snprintf(buf, sizeof(buf), "unix %s", path);
	} else if (strcmp(proto->s, "ip") == 0 || strcmp(proto->s, "tcp") == 0 || strcmp(proto->s, "udp") == 0 ||
	    strcmp(proto->s, "ip4") == 0 || strcmp(proto->s, "ip6") == 0 || strcmp(proto->s, "tcp4") == 0 ||
	    strcmp(proto->s, "tcp6") == 0 || strcmp(proto->s, "udp4") == 0 || strcmp(proto->s, "udp6") == 0)
		snprintf(buf, sizeof(buf), "ip %s", where != NULL && where->type == V_STR ? where->s : "*:*");
	else
		return filter_value(in, new_filter(in, SBF_NEVER));
	struct filter *f = new_filter(in, SBF_ADDR);
	f->kind = kind;
	f->str = dup_n(in, buf, strlen(buf));
	return filter_value(in, f);
}

static value *f_local(struct interp *in, value *a, struct env *e) { (void)e; return address_filter(in, a, SBA_LOCAL); }
static value *f_remote(struct interp *in, value *a, struct env *e) { (void)e; return address_filter(in, a, SBA_REMOTE); }

static value *combine(struct interp *in, value *args, int type) {
	struct filter *kids[64];
	int n = 0;
	for (; args->type == V_PAIR && n < 64; args = args->cdr)
		if (args->car->type == V_FILTER)
			kids[n++] = args->car->filter;
	if (type == SBF_NOT) {
		struct filter *f = new_filter(in, SBF_NOT);
		f->n = 1;
		f->kids = arena_alloc(&in->arena, sizeof(struct filter *));
		if (f->kids != NULL)
			f->kids[0] = n > 0 ? kids[0] : new_filter(in, SBF_NEVER);
		return filter_value(in, f);
	}
	if (n == 0)
		return filter_value(in, new_filter(in, type == SBF_ALL ? SBF_TRUE : SBF_NEVER));
	return filter_value(in, any_of(in, kids, n, type));
}

static value *f_require_all(struct interp *in, value *a, struct env *e) { (void)e; return combine(in, a, SBF_ALL); }
static value *f_require_any(struct interp *in, value *a, struct env *e) { (void)e; return combine(in, a, SBF_ANY); }
static value *f_require_not(struct interp *in, value *a, struct env *e) { (void)e; return combine(in, a, SBF_NOT); }

/* A filter this system does not know never matches. */
static value *f_unknown(struct interp *in, value *a, struct env *e) {
	(void)a;
	(void)e;
	return filter_value(in, new_filter(in, SBF_NEVER));
}

/* -- Rules -- */

static value *fn_with(struct interp *in, value *args, struct env *env) {
	(void)env;
	value *m = mk(in, V_MOD);
	value *what = arg(args, 0);
	m->s = what != NULL && what->type == V_SYM ? what->s : "";
	if (strcmp(m->s, "no-log") == 0)
		m->modflags = SB_FLAG_NO_LOG;
	else if (strcmp(m->s, "report") == 0)
		m->modflags = SB_FLAG_REPORT;
	return m;
}

static void add_rule(struct interp *in, const char *operation, int action, int flags, struct filter *f) {
	if (strcmp(operation, "default") == 0 && f == NULL) {
		in->default_action = action;
		in->default_flags = flags;
		in->has_default = 1;
		return;
	}
	if (in->nrules == in->rulecap) {
		size_t cap = in->rulecap ? in->rulecap * 2 : 64;
		struct rule *r = realloc(in->rules, cap * sizeof(*r));
		if (r == NULL) {
			fail(in, "out of memory");
			return;
		}
		in->rules = r;
		in->rulecap = cap;
	}
	struct rule *r = &in->rules[in->nrules++];
	r->operation = operation;
	r->action = action;
	r->flags = flags;
	r->filter = f;
}

/* (allow OPERATION... FILTER... (with ...)...): one rule per operation,
   its filters alternatives, ANDed with any with-filter around it. */
static value *rule(struct interp *in, value *args, int action) {
	const char *ops[64];
	struct filter *filters[64];
	int nops = 0, nfilters = 0, flags = 0;
	for (; args->type == V_PAIR; args = args->cdr) {
		value *a = args->car;
		if (a->type == V_SYM && nops < 64)
			ops[nops++] = a->s;
		else if (a->type == V_FILTER && nfilters < 64)
			filters[nfilters++] = a->filter;
		else if (a->type == V_MOD)
			flags |= a->modflags;
		else if (a->type == V_PAIR) {	/* a list of operations or filters */
			for (value *b = a; b->type == V_PAIR; b = b->cdr) {
				if (b->car->type == V_SYM && nops < 64)
					ops[nops++] = b->car->s;
				else if (b->car->type == V_FILTER && nfilters < 64)
					filters[nfilters++] = b->car->filter;
			}
		}
	}
	struct filter *f = nfilters > 0 ? any_of(in, filters, nfilters, SBF_ANY) : NULL;
	if (in->ncontext > 0) {
		struct filter *all[17];
		int n = 0;
		for (int i = 0; i < in->ncontext; i++)
			all[n++] = in->context[i];
		if (f != NULL)
			all[n++] = f;
		f = any_of(in, all, n, SBF_ALL);
	}
	for (int i = 0; i < nops; i++)
		add_rule(in, ops[i], action, flags, strcmp(ops[i], "default") == 0 && in->ncontext == 0 ? NULL : f);
	return &in->nil;
}

static value *fn_allow(struct interp *in, value *a, struct env *e) { (void)e; return rule(in, a, SB_ALLOW); }
static value *fn_deny(struct interp *in, value *a, struct env *e) { (void)e; return rule(in, a, SB_DENY); }

static value *sf_with_filter(struct interp *in, value *args, struct env *env) {
	value *f = args->type == V_PAIR ? eval(in, args->car, env) : &in->nil;
	if (f->type != V_FILTER || in->ncontext >= 16)
		return eval_body(in, args->type == V_PAIR ? args->cdr : &in->nil, env);
	in->context[in->ncontext++] = f->filter;
	value *result = eval_body(in, args->cdr, env);
	in->ncontext--;
	return result;
}

static const struct {
	const char *name;
	builtin_fn fn;
	int special;
} builtins[] = {
	{ "quote", sf_quote, 1 }, { "define", sf_define, 1 }, { "lambda", sf_lambda, 1 },
	{ "if", sf_if, 1 }, { "when", sf_when, 1 }, { "unless", sf_unless, 1 }, { "begin", sf_begin, 1 },
	{ "let", sf_let, 1 }, { "let*", sf_let, 1 }, { "cond", sf_cond, 1 }, { "and", sf_and, 1 },
	{ "or", sf_or, 1 }, { "defined?", sf_defined, 1 }, { "with-filter", sf_with_filter, 1 },
	{ "version", fn_version, 0 }, { "param", fn_param, 0 }, { "string-append", fn_string_append, 0 },
	{ "string=?", fn_string_eq, 0 }, { "string-prefix?", fn_string_prefix, 0 }, { "string?", fn_stringp, 0 },
	{ "string->number", fn_string_to_number, 0 }, { "number->string", fn_number_to_string, 0 },
	{ "=", fn_num_eq, 0 }, { "<", fn_lt, 0 }, { ">", fn_gt, 0 }, { "<=", fn_le, 0 }, { ">=", fn_ge, 0 },
	{ "equal?", fn_equal, 0 }, { "eq?", fn_equal, 0 }, { "eqv?", fn_equal, 0 }, { "not", fn_not, 0 },
	{ "list", fn_list, 0 }, { "debug", fn_ignore, 0 }, { "trace", fn_ignore, 0 },
	{ "import", fn_ignore, 0 }, { "regex-quote", fn_regex_quote, 0 },
	{ "allow", fn_allow, 0 }, { "deny", fn_deny, 0 }, { "with", fn_with, 0 },
	{ "literal", f_literal, 0 }, { "path-literal", f_literal, 0 }, { "subpath", f_subpath, 0 },
	{ "prefix", f_prefix, 0 }, { "path-prefix", f_prefix, 0 }, { "regex", f_regex, 0 },
	{ "path-regex", f_regex, 0 },
	{ "vnode-type", f_vnode_type, 0 }, { "target", f_target, 0 }, { "socket-domain", f_socket_domain, 0 },
	{ "socket-type", f_socket_type, 0 }, { "socket-protocol", f_socket_protocol, 0 },
	{ "file-mode", f_file_mode, 0 }, { "local", f_local, 0 }, { "remote", f_remote, 0 },
	{ "require-all", f_require_all, 0 }, { "require-any", f_require_any, 0 },
	{ "require-not", f_require_not, 0 },
	/* Filters on what this system does not check: never match. */
	{ "signing-identifier", f_unknown, 0 }, { "entitlement-is-present", f_unknown, 0 },
	{ "entitlement-is-bool-true", f_unknown, 0 }, { "process-path", f_unknown, 0 },
	{ "appleevent-destination", f_unknown, 0 }, { "right-name", f_unknown, 0 },
	{ "notification-name", f_unknown, 0 }, { "kext-bundle-id", f_unknown, 0 },
	{ "info-type", f_unknown, 0 }, { "device-conforms-to", f_unknown, 0 },
	{ "mount-relative-path", f_unknown, 0 }, { "debug-mode", f_unknown, 0 },
};

static value *name_filter_fn(struct interp *in, value *args, struct env *env);

/* The name filters, also with -regex and -prefix. */
static void define_builtins(struct interp *in, struct env *env) {
	for (size_t i = 0; i < sizeof(builtins) / sizeof(builtins[0]); i++) {
		value *v = mk(in, V_BUILTIN);
		v->fn = builtins[i].fn;
		v->special = builtins[i].special;
		v->s = builtins[i].name;
		bind(in, env, builtins[i].name, v);
	}
	static const char *const suffixes[] = { "", "-regex", "-prefix" };
	static const int matches[] = { SBM_LITERAL, SBM_REGEX, SBM_PREFIX };
	for (size_t i = 0; i < sizeof(name_filters) / sizeof(name_filters[0]); i++)
		for (int j = 0; j < 3; j++) {
			char name[64];
			snprintf(name, sizeof(name), "%s%s", name_filters[i].name, suffixes[j]);
			value *v = mk(in, V_BUILTIN);
			v->fn = name_filter_fn;
			v->s = dup_n(in, name, strlen(name));
			v->modflags = name_filters[i].kind << 8 | matches[j];
			bind(in, env, v->s, v);
		}
}

/* The name filters (global-name, sysctl-name-regex, ...): the builtin
   applied carries its kind and match. */
static value *name_filter_fn(struct interp *in, value *args, struct env *env) {
	(void)env;
	int code = in->current != NULL ? in->current->modflags : (SBN_GLOBAL_NAME << 8 | SBM_LITERAL);
	return string_filter(in, args, SBF_NAME, code >> 8, code & 0xff);
}

/* ---- Compiling to the blob ---- */

struct writer {
	uint8_t *filters;
	size_t nfilters, capfilters;
	char *strings;
	size_t nstrings, capstrings;
	int failed;
};

static void put_bytes(uint8_t **buf, size_t *n, size_t *cap, const void *p, size_t len, int *failed) {
	if (*n + len > *cap) {
		size_t c = *cap ? *cap : 1024;
		while (c < *n + len)
			c *= 2;
		uint8_t *b = realloc(*buf, c);
		if (b == NULL) {
			*failed = 1;
			return;
		}
		*buf = b;
		*cap = c;
	}
	memcpy(*buf + *n, p, len);
	*n += len;
}

static uint32_t intern(struct writer *w, const char *s) {
	/* Strings repeat (operations above all): one copy each. */
	size_t len = strlen(s) + 1;
	for (size_t off = 0; off < w->nstrings;) {
		size_t l = strlen(w->strings + off) + 1;
		if (l == len && memcmp(w->strings + off, s, len) == 0)
			return (uint32_t)off;
		off += l;
	}
	uint32_t off = (uint32_t)w->nstrings;
	put_bytes((uint8_t **)&w->strings, &w->nstrings, &w->capstrings, s, len, &w->failed);
	return off;
}

static void put_u8(struct writer *w, uint8_t v) {
	put_bytes(&w->filters, &w->nfilters, &w->capfilters, &v, 1, &w->failed);
}

static void put_u16(struct writer *w, uint16_t v) {
	uint8_t b[2] = { (uint8_t)v, (uint8_t)(v >> 8) };
	put_bytes(&w->filters, &w->nfilters, &w->capfilters, b, 2, &w->failed);
}

static void put_u32(struct writer *w, uint32_t v) {
	uint8_t b[4] = { (uint8_t)v, (uint8_t)(v >> 8), (uint8_t)(v >> 16), (uint8_t)(v >> 24) };
	put_bytes(&w->filters, &w->nfilters, &w->capfilters, b, 4, &w->failed);
}

static void write_filter(struct writer *w, const struct filter *f) {
	put_u8(w, (uint8_t)f->type);
	switch (f->type) {
	case SBF_ALL:
	case SBF_ANY:
		put_u16(w, (uint16_t)f->n);
		for (int i = 0; i < f->n; i++)
			write_filter(w, f->kids[i]);
		break;
	case SBF_NOT:
		write_filter(w, f->kids[0]);
		break;
	case SBF_PATH:
		put_u8(w, (uint8_t)f->match);
		put_u32(w, intern(w, f->str));
		break;
	case SBF_NAME:
		put_u8(w, (uint8_t)f->kind);
		put_u8(w, (uint8_t)f->match);
		put_u32(w, intern(w, f->str));
		break;
	case SBF_INT:
		put_u8(w, (uint8_t)f->kind);
		put_u32(w, f->value);
		break;
	case SBF_ADDR:
		put_u8(w, (uint8_t)f->kind);
		put_u32(w, intern(w, f->str));
		break;
	}
}

static uint8_t *write_blob(struct interp *in, size_t *size) {
	struct writer w;
	memset(&w, 0, sizeof(w));
	struct sb_rule *rules = calloc(in->nrules ? in->nrules : 1, sizeof(struct sb_rule));
	if (rules == NULL)
		return NULL;
	for (size_t i = 0; i < in->nrules; i++) {
		rules[i].operation = intern(&w, in->rules[i].operation);
		rules[i].action = (uint8_t)in->rules[i].action;
		rules[i].flags = (uint8_t)in->rules[i].flags;
		if (in->rules[i].filter == NULL)
			rules[i].filter = SB_NO_FILTER;
		else {
			rules[i].filter = (uint32_t)w.nfilters;
			write_filter(&w, in->rules[i].filter);
		}
	}
	if (w.nstrings == 0)
		intern(&w, "");
	struct sb_header h;
	memset(&h, 0, sizeof(h));
	h.magic = SB_MAGIC;
	h.version = SB_VERSION;
	h.default_action = in->has_default ? (uint32_t)in->default_action : SB_ALLOW;
	h.default_flags = (uint32_t)in->default_flags;
	h.nrules = (uint32_t)in->nrules;
	h.rules_offset = sizeof(h);
	h.filters_offset = h.rules_offset + (uint32_t)(in->nrules * sizeof(struct sb_rule));
	h.strings_offset = h.filters_offset + (uint32_t)w.nfilters;
	h.size = h.strings_offset + (uint32_t)w.nstrings;
	uint8_t *blob = NULL;
	if (!w.failed && h.size <= SB_MAX_BLOB)
		blob = malloc(h.size);
	if (blob != NULL) {
		memcpy(blob, &h, sizeof(h));
		memcpy(blob + h.rules_offset, rules, in->nrules * sizeof(struct sb_rule));
		memcpy(blob + h.filters_offset, w.filters, w.nfilters);
		memcpy(blob + h.strings_offset, w.strings, w.nstrings);
		*size = h.size;
	} else
		snprintf(in->error, sizeof(in->error), w.failed ? "out of memory" : "the profile is too large");
	free(rules);
	free(w.filters);
	free(w.strings);
	return blob;
}

/* ---- The entry point ---- */

uint8_t *sbpl_compile(const char *source, const char *const *params, size_t *size, char *error, size_t errorsize) {
	struct interp in;
	memset(&in, 0, sizeof(in));
	in.params = params;
	in.nil.type = V_NIL;
	in.vtrue.type = V_TRUE;
	in.vfalse.type = V_FALSE;
	struct env *global = new_env(&in, NULL);
	define_builtins(&in, global);

	struct reader r = { source, 1 };
	for (;;) {
		skip_space(&r);
		if (*r.p == '\0' || in.failed)
			break;
		in.depth = 0;
		value *form = read_form(&in, &r);
		in.depth = 0;
		eval(&in, form, global);
	}
	uint8_t *blob = NULL;
	if (!in.failed)
		blob = write_blob(&in, size);
	if (blob == NULL && error != NULL)
		snprintf(error, errorsize, "%s", in.error[0] ? in.error : "the profile could not be compiled");
	free(in.rules);
	arena_free(&in.arena);
	return blob;
}
