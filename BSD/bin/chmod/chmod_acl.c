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
 * chmod's ACL operations, as macOS documents them in chmod(1):
 *
 *   +a ACE		add an entry in canonical order: explicit deny,
 *			explicit allow, inherited deny, inherited allow; an
 *			entry for the same user or group, kind and
 *			inheritance gains the new permissions instead
 *   +a# n ACE		insert an entry at position n
 *   -a ACE		take the ACE's permissions from the matching entry,
 *			which goes away when none are left
 *   -a# n		delete entry n
 *   =a# n ACE		replace entry n
 *   -N			remove the ACL
 *   -E			replace the ACL with the ACEs on standard input,
 *			one per line
 *   -C			fail if an ACL is not in canonical order
 *   -i			clear the inherited flag of every entry
 *   -I			delete the inherited entries
 *
 * An ACE is "[user:|group:]name [inherited] allow|deny perm[,perm...]".
 * A name without a prefix is a user, or else a group. The permissions
 * are those of chmod(1) and ls -le, and the list also takes the
 * inheritance flags. Names become UUIDs through the membership calls.
 */

#include <sys/types.h>
#include <sys/acl.h>
#include <sys/stat.h>

#include <ctype.h>
#include <err.h>
#include <errno.h>
#include <grp.h>
#include <membership.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include "chmod_acl.h"

static const struct {
	const char	*name;
	acl_perm_t	perm;
} perms[] = {
	{ "read",		ACL_READ_DATA },
	{ "list",		ACL_LIST_DIRECTORY },
	{ "write",		ACL_WRITE_DATA },
	{ "add_file",		ACL_ADD_FILE },
	{ "execute",		ACL_EXECUTE },
	{ "search",		ACL_SEARCH },
	{ "delete",		ACL_DELETE },
	{ "append",		ACL_APPEND_DATA },
	{ "add_subdirectory",	ACL_ADD_SUBDIRECTORY },
	{ "delete_child",	ACL_DELETE_CHILD },
	{ "readattr",		ACL_READ_ATTRIBUTES },
	{ "writeattr",		ACL_WRITE_ATTRIBUTES },
	{ "readextattr",	ACL_READ_EXTATTRIBUTES },
	{ "writeextattr",	ACL_WRITE_EXTATTRIBUTES },
	{ "readsecurity",	ACL_READ_SECURITY },
	{ "writesecurity",	ACL_WRITE_SECURITY },
	{ "chown",		ACL_CHANGE_OWNER },
	{ "synchronize",	ACL_SYNCHRONIZE },
};

static const struct {
	const char	*name;
	acl_flag_t	flag;
} flags[] = {
	{ "file_inherit",	ACL_ENTRY_FILE_INHERIT },
	{ "directory_inherit",	ACL_ENTRY_DIRECTORY_INHERIT },
	{ "limit_inherit",	ACL_ENTRY_LIMIT_INHERIT },
	{ "only_inherit",	ACL_ENTRY_ONLY_INHERIT },
};

#define	NELEM(a)	(sizeof(a) / sizeof((a)[0]))

enum chmod_acl_op
chmod_acl_op_of(const char *arg)
{
	if (strcmp(arg, "+a") == 0)
		return (CHMOD_ACL_ADD);
	if (strcmp(arg, "+a#") == 0)
		return (CHMOD_ACL_ADD_AT);
	if (strcmp(arg, "-a") == 0)
		return (CHMOD_ACL_REMOVE);
	if (strcmp(arg, "-a#") == 0)
		return (CHMOD_ACL_REMOVE_AT);
	if (strcmp(arg, "=a#") == 0 || strcmp(arg, "=a") == 0)
		return (CHMOD_ACL_REPLACE_AT);
	return (CHMOD_ACL_NONE);
}

/* The UUID of a user or group name ("user:", "group:" or either). */
static void
name_to_uuid(const char *name, uuid_t uu, int *isgroup)
{
	struct passwd *pw;
	struct group *gr;
	const char *n = name;
	int want = 0;		/* 0 either, 1 user, 2 group */
	char *end;
	long id;

	if (strncmp(n, "user:", 5) == 0) {
		n += 5;
		want = 1;
	} else if (strncmp(n, "group:", 6) == 0) {
		n += 6;
		want = 2;
	}
	if (want != 2 && (pw = getpwnam(n)) != NULL) {
		if (mbr_uid_to_uuid(pw->pw_uid, uu) != 0)
			errx(1, "%s: cannot map the user to a UUID", n);
		*isgroup = 0;
		return;
	}
	if (want != 1 && (gr = getgrnam(n)) != NULL) {
		if (mbr_gid_to_uuid(gr->gr_gid, uu) != 0)
			errx(1, "%s: cannot map the group to a UUID", n);
		*isgroup = 1;
		return;
	}
	/* A numeric id, as a user unless "group:" says otherwise. */
	errno = 0;
	id = strtol(n, &end, 10);
	if (*n != '\0' && *end == '\0' && errno == 0 && id >= 0) {
		if ((want == 2 ? mbr_gid_to_uuid((gid_t)id, uu) :
		    mbr_uid_to_uuid((uid_t)id, uu)) != 0)
			errx(1, "%s: cannot map the id to a UUID", n);
		*isgroup = want == 2;
		return;
	}
	errx(1, "%s: no such %s", n, want == 1 ? "user" : want == 2 ?
	    "group" : "user or group");
}

/* Parses one ACE into a new entry of acl. */
static void
parse_ace(acl_t *aclp, const char *text, int at)
{
	char *copy, *word, *rest, *list, *p;
	acl_entry_t e;
	acl_permset_t ps;
	acl_flagset_t fs;
	uuid_t uu;
	int isgroup, inherited = 0, kind = -1;
	size_t i;

	if ((copy = strdup(text)) == NULL)
		err(1, NULL);
	rest = copy;
	while (isspace((unsigned char)*rest))
		rest++;
	if ((word = strsep(&rest, " \t")) == NULL || *word == '\0')
		errx(1, "%s: missing user or group", text);
	name_to_uuid(word, uu, &isgroup);
	while (rest != NULL && kind == -1) {
		word = strsep(&rest, " \t");
		if (*word == '\0')
			continue;
		if (strcmp(word, "inherited") == 0)
			inherited = 1;
		else if (strcmp(word, "allow") == 0)
			kind = ACL_EXTENDED_ALLOW;
		else if (strcmp(word, "deny") == 0)
			kind = ACL_EXTENDED_DENY;
		else
			errx(1, "%s: expected allow or deny, not \"%s\"",
			    text, word);
	}
	if (kind == -1)
		errx(1, "%s: missing allow or deny", text);
	if (rest == NULL)
		errx(1, "%s: missing permissions", text);

	if (acl_create_entry_np(aclp, &e, at) != 0)
		err(1, "acl_create_entry_np");
	if (acl_set_tag_type(e, kind) != 0 || acl_set_qualifier(e, uu) != 0)
		err(1, "%s", text);
	if (acl_get_permset(e, &ps) != 0 || acl_clear_perms(ps) != 0 ||
	    acl_get_flagset_np(e, &fs) != 0 || acl_clear_flags_np(fs) != 0)
		err(1, "%s", text);
	if (inherited)
		acl_add_flag_np(fs, ACL_ENTRY_INHERITED);

	/* The rest is a comma-separated list; spaces around commas are fine. */
	list = rest;
	while ((p = strsep(&list, ",")) != NULL) {
		while (isspace((unsigned char)*p))
			p++;
		for (word = p + strlen(p); word > p &&
		    isspace((unsigned char)word[-1]); word--)
			word[-1] = '\0';
		if (*p == '\0')
			continue;
		for (i = 0; i < NELEM(perms); i++)
			if (strcmp(p, perms[i].name) == 0)
				break;
		if (i < NELEM(perms)) {
			acl_add_perm(ps, perms[i].perm);
			continue;
		}
		for (i = 0; i < NELEM(flags); i++)
			if (strcmp(p, flags[i].name) == 0)
				break;
		if (i == NELEM(flags))
			errx(1, "%s: unknown permission \"%s\"", text, p);
		acl_add_flag_np(fs, flags[i].flag);
	}
	if (acl_set_permset(e, ps) != 0 || acl_set_flagset_np(e, fs) != 0)
		err(1, "%s", text);
	(void)isgroup;
	free(copy);
}

static int
parse_index(const char *s)
{
	char *end;
	long n;

	errno = 0;
	n = strtol(s, &end, 10);
	if (*s == '\0' || *end != '\0' || errno != 0 || n < 0 ||
	    n >= ACL_MAX_ENTRIES)
		errx(1, "%s: invalid ACL entry index", s);
	return ((int)n);
}

void
chmod_acl_prepare(struct chmod_acl_req *req, enum chmod_acl_op op,
    int *argcp, char ***argvp, int consume_op)
{
	char **argv = *argvp;
	int argc = *argcp;
	char *line = NULL;
	size_t cap = 0;
	ssize_t len;

	memset(req, 0, sizeof(*req));
	req->op = op;
	if (consume_op) {
		argv++;
		argc--;
	}
	if (op == CHMOD_ACL_ADD_AT || op == CHMOD_ACL_REMOVE_AT ||
	    op == CHMOD_ACL_REPLACE_AT) {
		if (argc < 1)
			errx(1, "missing ACL entry index");
		req->index = parse_index(*argv);
		argv++;
		argc--;
	}
	if (op == CHMOD_ACL_ADD || op == CHMOD_ACL_ADD_AT ||
	    op == CHMOD_ACL_REMOVE || op == CHMOD_ACL_REPLACE_AT) {
		if (argc < 1)
			errx(1, "missing ACL entry");
		if ((req->entries = acl_init(1)) == NULL)
			err(1, "acl_init");
		parse_ace(&req->entries, *argv, ACL_LAST_ENTRY);
		argv++;
		argc--;
	} else if (op == CHMOD_ACL_SET) {
		if ((req->entries = acl_init(ACL_MAX_ENTRIES)) == NULL)
			err(1, "acl_init");
		while ((len = getline(&line, &cap, stdin)) > 0) {
			if (line[len - 1] == '\n')
				line[--len] = '\0';
			if (strspn(line, " \t") == (size_t)len)
				continue;
			parse_ace(&req->entries, line, ACL_LAST_ENTRY);
		}
		free(line);
	}
	*argvp = argv;
	*argcp = argc;
}

static int
entry_kind(acl_entry_t e)
{
	acl_tag_t tag;

	return (acl_get_tag_type(e, &tag) == 0 ? (int)tag : -1);
}

static int
entry_inherited(acl_entry_t e)
{
	acl_flagset_t fs;

	return (acl_get_flagset_np(e, &fs) == 0 &&
	    acl_get_flag_np(fs, ACL_ENTRY_INHERITED) == 1);
}

/* 0 explicit deny, 1 explicit allow, 2 inherited deny, 3 inherited allow. */
static int
entry_rank(acl_entry_t e)
{
	return (entry_inherited(e) * 2 +
	    (entry_kind(e) == ACL_EXTENDED_ALLOW ? 1 : 0));
}

/* Same user or group, same allow/deny and same inheritance. */
static int
entry_matches(acl_entry_t a, acl_entry_t b)
{
	void *qa, *qb;
	int same;

	if (entry_kind(a) != entry_kind(b) ||
	    entry_inherited(a) != entry_inherited(b))
		return (0);
	qa = acl_get_qualifier(a);
	qb = acl_get_qualifier(b);
	same = qa != NULL && qb != NULL && memcmp(qa, qb, sizeof(uuid_t)) == 0;
	acl_free(qa);
	acl_free(qb);
	return (same);
}

static int
entry_at(acl_t acl, int index, acl_entry_t *ep)
{
	int i, id = ACL_FIRST_ENTRY;

	for (i = 0; acl_get_entry(acl, id, ep) == 0; i++) {
		if (i == index)
			return (0);
		id = ACL_NEXT_ENTRY;
	}
	return (-1);
}

static int
entry_count(acl_t acl)
{
	acl_entry_t e;
	int n = 0, id = ACL_FIRST_ENTRY;

	while (acl_get_entry(acl, id, &e) == 0) {
		n++;
		id = ACL_NEXT_ENTRY;
	}
	return (n);
}

static void
copy_entry_at(acl_t *aclp, int at, acl_entry_t src)
{
	acl_entry_t e;

	if (acl_create_entry_np(aclp, &e, at) != 0 || acl_copy_entry(e, src) != 0)
		err(1, "acl_copy_entry");
}

/* Adds or takes away src's permissions and inheritance flags. */
static void
merge_entry(acl_entry_t dst, acl_entry_t src, int add)
{
	acl_permset_mask_t dm, sm;
	acl_flagset_t df, sf;
	size_t i;

	if (acl_get_permset_mask_np(dst, &dm) != 0 ||
	    acl_get_permset_mask_np(src, &sm) != 0)
		err(1, "acl_get_permset_mask_np");
	dm = add ? (dm | sm) : (dm & ~sm);
	if (acl_set_permset_mask_np(dst, dm) != 0)
		err(1, "acl_set_permset_mask_np");
	if (acl_get_flagset_np(dst, &df) != 0 || acl_get_flagset_np(src, &sf) != 0)
		err(1, "acl_get_flagset_np");
	for (i = 0; i < NELEM(flags); i++) {
		if (acl_get_flag_np(sf, flags[i].flag) != 1)
			continue;
		if (add)
			acl_add_flag_np(df, flags[i].flag);
		else
			acl_delete_flag_np(df, flags[i].flag);
	}
	if (acl_set_flagset_np(dst, df) != 0)
		err(1, "acl_set_flagset_np");
}

static int
entry_empty(acl_entry_t e)
{
	acl_permset_mask_t m;

	return (acl_get_permset_mask_np(e, &m) == 0 && m == 0);
}

int
chmod_acl_apply(const struct chmod_acl_req *req, const char *path,
    const char *shown, int follow)
{
	acl_t acl;
	acl_entry_t want = NULL, e;
	int i, n, rank, id, changed = 1;

	acl = follow ? acl_get_file(path, ACL_TYPE_EXTENDED) :
	    acl_get_link_np(path, ACL_TYPE_EXTENDED);
	if (acl == NULL) {
		/* Darwin reports a file without an ACL as ENOENT. */
		if (errno != ENOENT) {
			warn("%s", shown);
			return (1);
		}
		if ((acl = acl_init(1)) == NULL)
			err(1, "acl_init");
	}
	if (req->entries != NULL)
		(void)acl_get_entry(req->entries, ACL_FIRST_ENTRY, &want);
	n = entry_count(acl);

	switch (req->op) {
	case CHMOD_ACL_ADD:
		for (id = ACL_FIRST_ENTRY; acl_get_entry(acl, id, &e) == 0;
		    id = ACL_NEXT_ENTRY)
			if (entry_matches(e, want)) {
				merge_entry(e, want, 1);
				goto set;
			}
		rank = entry_rank(want);
		for (i = 0; i < n; i++) {
			(void)entry_at(acl, i, &e);
			if (entry_rank(e) > rank)
				break;
		}
		copy_entry_at(&acl, i, want);
		break;
	case CHMOD_ACL_ADD_AT:
		if (req->index > n)
			goto range;
		copy_entry_at(&acl, req->index, want);
		break;
	case CHMOD_ACL_REMOVE:
		for (i = 0; i < n; i++) {
			(void)entry_at(acl, i, &e);
			if (entry_matches(e, want))
				break;
		}
		if (i == n) {
			warnx("%s: no matching ACL entry", shown);
			acl_free(acl);
			return (1);
		}
		merge_entry(e, want, 0);
		if (entry_empty(e))
			acl_delete_entry(acl, e);
		break;
	case CHMOD_ACL_REMOVE_AT:
		if (entry_at(acl, req->index, &e) != 0)
			goto range;
		acl_delete_entry(acl, e);
		break;
	case CHMOD_ACL_REPLACE_AT:
		if (entry_at(acl, req->index, &e) != 0)
			goto range;
		if (acl_copy_entry(e, want) != 0)
			err(1, "acl_copy_entry");
		break;
	case CHMOD_ACL_CLEAR:
		acl_free(acl);
		if ((acl = acl_init(1)) == NULL)
			err(1, "acl_init");
		changed = n > 0;
		break;
	case CHMOD_ACL_SET:
		acl_free(acl);
		if ((acl = acl_dup(req->entries)) == NULL)
			err(1, "acl_dup");
		break;
	case CHMOD_ACL_CHECK:
		changed = 0;
		for (i = 0, rank = 0; i < n; i++) {
			(void)entry_at(acl, i, &e);
			if (entry_rank(e) < rank) {
				warnx("%s: the ACL is not in canonical order",
				    shown);
				acl_free(acl);
				return (1);
			}
			rank = entry_rank(e);
		}
		break;
	case CHMOD_ACL_UNINHERIT:
		for (i = 0; i < n; i++) {
			acl_flagset_t fs;

			(void)entry_at(acl, i, &e);
			if (acl_get_flagset_np(e, &fs) == 0) {
				acl_delete_flag_np(fs, ACL_ENTRY_INHERITED);
				acl_set_flagset_np(e, fs);
			}
		}
		break;
	case CHMOD_ACL_DROP_INHERITED:
		for (i = n - 1; i >= 0; i--) {
			(void)entry_at(acl, i, &e);
			if (entry_inherited(e))
				acl_delete_entry(acl, e);
		}
		break;
	case CHMOD_ACL_NONE:
		changed = 0;
		break;
	}
set:
	if (changed && (follow ? acl_set_file(path, ACL_TYPE_EXTENDED, acl) :
	    acl_set_link_np(path, ACL_TYPE_EXTENDED, acl)) != 0) {
		warn("%s", shown);
		acl_free(acl);
		return (1);
	}
	acl_free(acl);
	return (0);
range:
	warnx("%s: no ACL entry %d", shown, req->index);
	acl_free(acl);
	return (1);
}
