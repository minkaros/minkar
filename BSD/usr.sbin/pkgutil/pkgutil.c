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
 * pkgutil: the receipts of installed packages. Each package the installer
 * lays down leaves in <volume>/var/db/receipts a property list (its
 * identifier, version, install location and date, groups) and a bill of
 * materials of its files; pkgutil lists and describes them, finds which
 * packages installed a path, and forgets a package by removing its
 * receipt. The files in a bill of materials are read with lsbom(8).
 * Expanding, flattening and checking flat .pkg files (xar archives) is
 * not done yet.
 */

#include <CoreFoundation/CoreFoundation.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <regex.h>
#include <spawn.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

extern char **environ;

#define LSBOM	"/usr/bin/lsbom"

static const char *volume = "/";
static char receipts[PATH_MAX];
static bool regexp_ids, only_files, only_dirs, force, verbose;

struct receipt {
	char		*id;
	CFDictionaryRef	 plist;
};

/* ---- Receipts ------------------------------------------------------------ */

static CFStringRef
cfstr(const char *s)
{
	return (CFStringCreateWithCString(NULL, s, kCFStringEncodingUTF8));
}

static CFDictionaryRef
read_plist(const char *path)
{
	CFURLRef url = CFURLCreateFromFileSystemRepresentation(NULL,
	    (const UInt8 *)path, (CFIndex)strlen(path), false);
	CFReadStreamRef rs = CFReadStreamCreateWithFile(NULL, url);
	CFPropertyListRef pl = NULL;

	if (rs != NULL && CFReadStreamOpen(rs)) {
		pl = CFPropertyListCreateWithStream(NULL, rs, 0,
		    kCFPropertyListImmutable, NULL, NULL);
		CFReadStreamClose(rs);
	}
	if (rs != NULL)
		CFRelease(rs);
	CFRelease(url);
	if (pl != NULL && CFGetTypeID(pl) != CFDictionaryGetTypeID()) {
		CFRelease(pl);
		pl = NULL;
	}
	return (pl);
}

static int
cmp_receipt(const void *a, const void *b)
{
	return (strcmp(((const struct receipt *)a)->id,
	    ((const struct receipt *)b)->id));
}

/* All the receipts on the volume, sorted by identifier. */
static struct receipt *
read_receipts(size_t *countp)
{
	struct receipt *r = NULL;
	size_t n = 0;
	struct dirent *de;
	DIR *d;

	*countp = 0;
	if ((d = opendir(receipts)) == NULL)
		return (NULL);
	while ((de = readdir(d)) != NULL) {
		size_t len = strlen(de->d_name);
		char path[PATH_MAX];
		CFDictionaryRef pl;

		if (len <= 6 || strcmp(de->d_name + len - 6, ".plist") != 0)
			continue;
		snprintf(path, sizeof(path), "%s/%s", receipts, de->d_name);
		if ((pl = read_plist(path)) == NULL)
			continue;
		r = realloc(r, sizeof(*r) * (n + 1));
		if (r == NULL)
			err(1, NULL);
		r[n].id = strndup(de->d_name, len - 6);
		r[n].plist = pl;
		n++;
	}
	closedir(d);
	qsort(r, n, sizeof(*r), cmp_receipt);
	*countp = n;
	return (r);
}

static bool
get_string(CFDictionaryRef d, const char *key, char *buf, size_t len)
{
	CFStringRef k = cfstr(key);
	CFTypeRef v = CFDictionaryGetValue(d, k);

	CFRelease(k);
	buf[0] = '\0';
	if (v == NULL || CFGetTypeID(v) != CFStringGetTypeID())
		return (false);
	return (CFStringGetCString(v, buf, (CFIndex)len, kCFStringEncodingUTF8));
}

static long long
install_time(CFDictionaryRef d)
{
	CFTypeRef v = CFDictionaryGetValue(d, CFSTR("InstallDate"));

	if (v == NULL || CFGetTypeID(v) != CFDateGetTypeID())
		return (0);
	return ((long long)(CFDateGetAbsoluteTime(v) +
	    kCFAbsoluteTimeIntervalSince1970));
}

static CFArrayRef
groups(CFDictionaryRef d)
{
	CFTypeRef v = CFDictionaryGetValue(d, CFSTR("PackageGroups"));

	if (v == NULL || CFGetTypeID(v) != CFArrayGetTypeID())
		return (NULL);
	return (v);
}

/* A package's install location, "/" when the receipt has none. */
static void
location(CFDictionaryRef d, char *buf, size_t len)
{
	if (!get_string(d, "InstallPrefixPath", buf, len) || buf[0] == '\0')
		strlcpy(buf, "/", len);
}

static void
print_plist(CFPropertyListRef pl)
{
	CFDataRef data = CFPropertyListCreateData(NULL, pl,
	    kCFPropertyListXMLFormat_v1_0, 0, NULL);

	if (data != NULL) {
		fwrite(CFDataGetBytePtr(data), 1, (size_t)CFDataGetLength(data),
		    stdout);
		CFRelease(data);
	}
}

/* An identifier given on the command line: a name, or with --regexp a
 * pattern the whole identifier must match. */
static bool
id_matches(const char *pattern, const char *id, bool is_regexp)
{
	char anchored[1024];
	regex_t re;
	bool m;

	if (!is_regexp)
		return (strcmp(pattern, id) == 0);
	snprintf(anchored, sizeof(anchored), "^(%s)$", pattern);
	if (regcomp(&re, anchored, REG_EXTENDED | REG_NOSUB) != 0)
		errx(1, "Invalid regular expression: %s", pattern);
	m = regexec(&re, id, 0, NULL, 0) == 0;
	regfree(&re);
	return (m);
}

/* The receipts an identifier argument names; none is an error. */
static size_t
select_receipts(struct receipt *r, size_t n, const char *arg, bool is_regexp,
    size_t *sel)
{
	size_t i, k = 0;

	for (i = 0; i < n; i++)
		if (id_matches(arg, r[i].id, is_regexp))
			sel[k++] = i;
	if (k == 0)
		fprintf(stderr, "No receipt for '%s' found at '%s'.\n", arg,
		    volume);
	return (k);
}

/* ---- The bill of materials, through lsbom ------------------------------- */

/* Runs lsbom with the given arguments, its output to a stream. */
static FILE *
lsbom(const char *const argv[], pid_t *pidp)
{
	posix_spawn_file_actions_t fa;
	int fds[2];
	FILE *f;

	if (pipe(fds) != 0)
		err(1, "pipe");
	posix_spawn_file_actions_init(&fa);
	posix_spawn_file_actions_adddup2(&fa, fds[1], STDOUT_FILENO);
	posix_spawn_file_actions_addclose(&fa, fds[0]);
	posix_spawn_file_actions_addclose(&fa, fds[1]);
	if (posix_spawn(pidp, LSBOM, &fa, NULL, (char *const *)argv,
	    environ) != 0)
		err(1, "%s", LSBOM);
	posix_spawn_file_actions_destroy(&fa);
	close(fds[1]);
	if ((f = fdopen(fds[0], "r")) == NULL)
		err(1, "fdopen");
	return (f);
}

static void
lsbom_done(FILE *f, pid_t pid)
{
	int status;

	fclose(f);
	while (waitpid(pid, &status, 0) < 0 && errno == EINTR)
		;
}

/* "./usr/bin/x" -> "usr/bin/x"; "." -> "" (the install location itself). */
static const char *
bom_path(char *line)
{
	line[strcspn(line, "\n")] = '\0';
	if (strcmp(line, ".") == 0)
		return ("");
	if (strncmp(line, "./", 2) == 0)
		return (line + 2);
	return (line);
}

static int
files(const char *id)
{
	char bom[PATH_MAX], line[PATH_MAX + 64];
	const char *argv[5];
	int argc = 0;
	pid_t pid;
	FILE *f;

	snprintf(bom, sizeof(bom), "%s/%s.bom", receipts, id);
	if (access(bom, R_OK) != 0) {
		fprintf(stderr, "No receipt for '%s' found at '%s'.\n", id,
		    volume);
		return (1);
	}
	argv[argc++] = "lsbom";
	argv[argc++] = "-s";
	if (only_files)
		argv[argc++] = "-fl";
	else if (only_dirs)
		argv[argc++] = "-d";
	argv[argc++] = bom;
	argv[argc] = NULL;
	f = lsbom(argv, &pid);
	while (fgets(line, sizeof(line), f) != NULL) {
		const char *p = bom_path(line);

		if (*p != '\0')
			printf("%s\n", p);
	}
	lsbom_done(f, pid);
	return (0);
}

/* ---- Commands ------------------------------------------------------------ */

static int
pkgs(const char *pattern, bool as_plist)
{
	struct receipt *r;
	CFMutableArrayRef a = NULL;
	size_t n, i;

	r = read_receipts(&n);
	if (as_plist)
		a = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	for (i = 0; i < n; i++) {
		if (pattern != NULL && !id_matches(pattern, r[i].id, true))
			continue;
		if (as_plist) {
			CFStringRef s = cfstr(r[i].id);

			CFArrayAppendValue(a, s);
			CFRelease(s);
		} else {
			printf("%s\n", r[i].id);
		}
	}
	if (as_plist) {
		print_plist(a);
		CFRelease(a);
	}
	return (0);
}

static void
groups_text(CFDictionaryRef d)
{
	CFArrayRef g = groups(d);
	CFIndex i;
	char buf[256];

	if (g == NULL || CFArrayGetCount(g) == 0)
		return;
	printf("groups:");
	for (i = 0; i < CFArrayGetCount(g); i++) {
		CFTypeRef v = CFArrayGetValueAtIndex(g, i);

		if (CFGetTypeID(v) == CFStringGetTypeID() &&
		    CFStringGetCString(v, buf, sizeof(buf),
		    kCFStringEncodingUTF8))
			printf(" %s", buf);
	}
	printf("\n");
}

static CFDictionaryRef
info_dict(const struct receipt *r)
{
	CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
	    &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks);
	char buf[PATH_MAX];
	long long t = install_time(r->plist);
	CFNumberRef num;
	CFStringRef s;
	CFArrayRef g;
	int one = 1;

	CFDictionarySetValue(d, CFSTR("pkgid"), (s = cfstr(r->id)));
	CFRelease(s);
	get_string(r->plist, "PackageVersion", buf, sizeof(buf));
	CFDictionarySetValue(d, CFSTR("pkg-version"), (s = cfstr(buf)));
	CFRelease(s);
	CFDictionarySetValue(d, CFSTR("volume"), (s = cfstr(volume)));
	CFRelease(s);
	location(r->plist, buf, sizeof(buf));
	CFDictionarySetValue(d, CFSTR("install-location"), (s = cfstr(buf)));
	CFRelease(s);
	num = CFNumberCreate(NULL, kCFNumberLongLongType, &t);
	CFDictionarySetValue(d, CFSTR("install-time"), num);
	CFRelease(num);
	num = CFNumberCreate(NULL, kCFNumberIntType, &one);
	CFDictionarySetValue(d, CFSTR("receipt-plist-version"), num);
	CFRelease(num);
	if ((g = groups(r->plist)) != NULL)
		CFDictionarySetValue(d, CFSTR("pkg-groups"), g);
	return (d);
}

static int
pkg_info(const char *arg, bool as_plist)
{
	struct receipt *r;
	size_t n, k, i, *sel;
	char buf[PATH_MAX];

	r = read_receipts(&n);
	sel = calloc(n + 1, sizeof(*sel));
	if ((k = select_receipts(r, n, arg, regexp_ids, sel)) == 0)
		return (1);
	for (i = 0; i < k; i++) {
		const struct receipt *e = &r[sel[i]];

		if (as_plist) {
			CFDictionaryRef d = info_dict(e);

			print_plist(d);
			CFRelease(d);
			continue;
		}
		printf("package-id: %s\n", e->id);
		get_string(e->plist, "PackageVersion", buf, sizeof(buf));
		printf("version: %s\n", buf);
		printf("volume: %s\n", volume);
		location(e->plist, buf, sizeof(buf));
		printf("location: %s\n", buf);
		printf("install-time: %lld\n", install_time(e->plist));
		groups_text(e->plist);
		if (i + 1 < k)
			printf("\n");
	}
	return (0);
}

/* The receipt's information and the paths it installed. */
static int
export_plist(const char *id)
{
	struct receipt *r;
	size_t n, k, *sel;
	CFMutableDictionaryRef d;
	CFMutableArrayRef paths;
	char bom[PATH_MAX], line[PATH_MAX + 64];
	const char *argv[] = { "lsbom", "-s", bom, NULL };
	pid_t pid;
	FILE *f;

	r = read_receipts(&n);
	sel = calloc(n + 1, sizeof(*sel));
	if ((k = select_receipts(r, n, id, false, sel)) == 0)
		return (1);
	d = CFDictionaryCreateMutableCopy(NULL, 0, info_dict(&r[sel[0]]));
	paths = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	snprintf(bom, sizeof(bom), "%s/%s.bom", receipts, id);
	if (access(bom, R_OK) == 0) {
		f = lsbom(argv, &pid);
		while (fgets(line, sizeof(line), f) != NULL) {
			const char *p = bom_path(line);
			CFStringRef s;

			if (*p == '\0')
				continue;
			CFArrayAppendValue(paths, (s = cfstr(p)));
			CFRelease(s);
		}
		lsbom_done(f, pid);
	}
	CFDictionarySetValue(d, CFSTR("paths"), paths);
	CFRelease(paths);
	print_plist(d);
	CFRelease(d);
	return (0);
}

/*
 * The packages that installed a path: each receipt whose bill of
 * materials has it, relative to the package's install location.
 */
static int
file_info(const char *arg, bool as_plist)
{
	struct receipt *r;
	size_t n, i;
	char want[PATH_MAX];
	CFMutableArrayRef a = NULL;

	/* Paths are absolute on the volume. */
	if (arg[0] == '/')
		strlcpy(want, arg + 1, sizeof(want));
	else
		strlcpy(want, arg, sizeof(want));
	while (want[0] != '\0' && want[strlen(want) - 1] == '/')
		want[strlen(want) - 1] = '\0';

	r = read_receipts(&n);
	if (as_plist)
		a = CFArrayCreateMutable(NULL, 0, &kCFTypeArrayCallBacks);
	else
		printf("volume: %s\npath: /%s\n", volume, want);
	for (i = 0; i < n; i++) {
		char bom[PATH_MAX], loc[PATH_MAX], line[PATH_MAX + 64];
		const char *argv[] = { "lsbom", "-p", "fmug", bom, NULL };
		const char *rel;
		size_t ll;
		pid_t pid;
		FILE *f;

		snprintf(bom, sizeof(bom), "%s/%s.bom", receipts, r[i].id);
		if (access(bom, R_OK) != 0)
			continue;
		location(r[i].plist, loc, sizeof(loc));
		/* The path inside the package: under its install location. */
		ll = strlen(loc + 1);
		if (ll == 0)
			rel = want;
		else if (strncmp(want, loc + 1, ll) == 0 &&
		    (want[ll] == '/' || want[ll] == '\0'))
			rel = want + ll + (want[ll] == '/');
		else
			continue;
		f = lsbom(argv, &pid);
		while (fgets(line, sizeof(line), f) != NULL) {
			char *path = line, *mode, *uid, *gid;
			char version[256];

			line[strcspn(line, "\n")] = '\0';
			if ((mode = strchr(path, '\t')) == NULL)
				continue;
			*mode++ = '\0';
			if ((uid = strchr(mode, '\t')) == NULL)
				continue;
			*uid++ = '\0';
			if ((gid = strchr(uid, '\t')) == NULL)
				continue;
			*gid++ = '\0';
			if (strcmp(bom_path(path), rel) != 0)
				continue;
			get_string(r[i].plist, "PackageVersion", version,
			    sizeof(version));
			if (as_plist) {
				CFMutableDictionaryRef d =
				    CFDictionaryCreateMutable(NULL, 0,
				    &kCFTypeDictionaryKeyCallBacks,
				    &kCFTypeDictionaryValueCallBacks);
				CFStringRef s;

				CFDictionarySetValue(d, CFSTR("pkgid"),
				    (s = cfstr(r[i].id)));
				CFRelease(s);
				CFDictionarySetValue(d, CFSTR("pkg-version"),
				    (s = cfstr(version)));
				CFRelease(s);
				CFDictionarySetValue(d, CFSTR("mode"),
				    (s = cfstr(mode)));
				CFRelease(s);
				CFArrayAppendValue(a, d);
				CFRelease(d);
			} else {
				printf("\npkgid: %s\npkg-version: %s\n"
				    "install-time: %lld\nuid: %s\ngid: %s\n"
				    "mode: %s\n", r[i].id, version,
				    install_time(r[i].plist), uid, gid, mode);
			}
			break;
		}
		lsbom_done(f, pid);
	}
	if (as_plist) {
		CFMutableDictionaryRef d = CFDictionaryCreateMutable(NULL, 0,
		    &kCFTypeDictionaryKeyCallBacks,
		    &kCFTypeDictionaryValueCallBacks);
		CFStringRef s;

		CFDictionarySetValue(d, CFSTR("volume"), (s = cfstr(volume)));
		CFRelease(s);
		CFDictionarySetValue(d, CFSTR("path"), (s = cfstr(arg)));
		CFRelease(s);
		CFDictionarySetValue(d, CFSTR("path-info"), a);
		CFRelease(a);
		print_plist(d);
		CFRelease(d);
	}
	return (0);
}

static int
forget(const char *arg)
{
	struct receipt *r;
	size_t n, k, i, *sel;
	int rc = 0;

	r = read_receipts(&n);
	sel = calloc(n + 1, sizeof(*sel));
	if ((k = select_receipts(r, n, arg, regexp_ids, sel)) == 0)
		return (1);
	for (i = 0; i < k; i++) {
		char path[PATH_MAX];
		const char *id = r[sel[i]].id;

		snprintf(path, sizeof(path), "%s/%s.plist", receipts, id);
		if (unlink(path) != 0) {
			warn("%s", path);
			rc = 1;
			continue;
		}
		snprintf(path, sizeof(path), "%s/%s.bom", receipts, id);
		(void)unlink(path);
		printf("Forgot package '%s' on '%s'.\n", id, volume);
	}
	return (rc);
}

/* Every group a receipt names, once, sorted. */
static CFArrayRef
all_groups(struct receipt *r, size_t n)
{
	CFMutableArrayRef a = CFArrayCreateMutable(NULL, 0,
	    &kCFTypeArrayCallBacks);
	size_t i;

	for (i = 0; i < n; i++) {
		CFArrayRef g = groups(r[i].plist);
		CFIndex j;

		for (j = 0; g != NULL && j < CFArrayGetCount(g); j++) {
			CFTypeRef v = CFArrayGetValueAtIndex(g, j);

			if (CFGetTypeID(v) == CFStringGetTypeID() &&
			    !CFArrayContainsValue(a,
			    CFRangeMake(0, CFArrayGetCount(a)), v))
				CFArrayAppendValue(a, v);
		}
	}
	CFArraySortValues(a, CFRangeMake(0, CFArrayGetCount(a)),
	    (CFComparatorFunction)CFStringCompare, NULL);
	return (a);
}

static void
print_strings(CFArrayRef a)
{
	char buf[1024];
	CFIndex i;

	for (i = 0; i < CFArrayGetCount(a); i++)
		if (CFStringGetCString(CFArrayGetValueAtIndex(a, i), buf,
		    sizeof(buf), kCFStringEncodingUTF8))
			printf("%s\n", buf);
}

static int
list_groups(bool as_plist)
{
	struct receipt *r;
	size_t n;
	CFArrayRef a;

	r = read_receipts(&n);
	a = all_groups(r, n);
	if (as_plist)
		print_plist(a);
	else
		print_strings(a);
	CFRelease(a);
	return (0);
}

static int
pkg_groups(const char *arg)
{
	struct receipt *r;
	size_t n, k, i, *sel;

	r = read_receipts(&n);
	sel = calloc(n + 1, sizeof(*sel));
	if ((k = select_receipts(r, n, arg, regexp_ids, sel)) == 0)
		return (1);
	for (i = 0; i < k; i++) {
		CFArrayRef g = groups(r[sel[i]].plist);

		if (g != NULL)
			print_strings(g);
	}
	return (0);
}

static int
group_pkgs(const char *group)
{
	struct receipt *r;
	size_t n, i;
	CFStringRef want = cfstr(group);
	int found = 0;

	r = read_receipts(&n);
	for (i = 0; i < n; i++) {
		CFArrayRef g = groups(r[i].plist);

		if (g != NULL && CFArrayContainsValue(g,
		    CFRangeMake(0, CFArrayGetCount(g)), want)) {
			printf("%s\n", r[i].id);
			found++;
		}
	}
	CFRelease(want);
	if (found == 0) {
		fprintf(stderr, "No packages in group '%s' found at '%s'.\n",
		    group, volume);
		return (1);
	}
	return (0);
}

static void
usage(void)
{
	fprintf(stderr,
	    "Usage: pkgutil [OPTIONS] [COMMANDS] ...\n\n"
	    "Options:\n"
	    "  --help                 Show this usage guide\n"
	    "  -v, --verbose          Show contextual information and format "
	    "for easy reading\n"
	    "  -f, --force            Perform all operations without asking "
	    "for confirmation\n"
	    "  --volume PATH          Perform all operations on the specified "
	    "volume or home directory\n"
	    "  --only-files           List only files (not directories) in "
	    "--files listing\n"
	    "  --only-dirs            List only directories (not files) in "
	    "--files listing\n"
	    "  --regexp               Try all PKG-ID arguments as regular "
	    "expressions\n\n"
	    "Receipt Database Commands:\n"
	    "  --pkgs, --packages     List all currently installed package IDs "
	    "on --volume\n"
	    "  --pkgs-plist           List all installed package IDs on "
	    "--volume in Mac OS X plist(5) format\n"
	    "  --pkgs=REGEXP          List package IDs on --volume that match "
	    "REGEXP\n"
	    "  --groups               List all GROUP-IDs on --volume\n"
	    "  --groups-plist         As --groups, but in plist(5) format\n"
	    "  --group-pkgs GROUP-ID  List all PKG-IDs in GROUP-ID\n"
	    "  --files PKG-ID         List files installed by the specified "
	    "package\n"
	    "  --export-plist PKG-ID  Print all info about PKG-ID in plist(5) "
	    "format\n"
	    "  --pkg-info PKG-ID      Show metadata about PKG-ID\n"
	    "  --pkg-info-plist PKG-ID  Show metadata about PKG-ID in plist(5) "
	    "format\n"
	    "  --pkg-groups PKG-ID    Show which groups PKG-ID belongs to\n"
	    "  --file-info PATH       Show metadata known about PATH\n"
	    "  --file-info-plist PATH As --file-info, but in plist(5) format\n"
	    "  --forget PKG-ID        Discard receipt data for the specified "
	    "package\n");
	exit(1);
}

enum {
	C_NONE, C_PKGS, C_PKGS_PLIST, C_GROUPS, C_GROUPS_PLIST, C_GROUP_PKGS,
	C_FILES, C_EXPORT, C_INFO, C_INFO_PLIST, C_PKG_GROUPS, C_FILE_INFO,
	C_FILE_INFO_PLIST, C_FORGET
};

int
main(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "help",		no_argument,		NULL, 'h' },
		{ "verbose",		no_argument,		NULL, 'v' },
		{ "force",		no_argument,		NULL, 'f' },
		{ "volume",		required_argument,	NULL, 'V' },
		{ "only-files",		no_argument,		NULL, 'F' },
		{ "only-dirs",		no_argument,		NULL, 'D' },
		{ "regexp",		no_argument,		NULL, 'r' },
		{ "pkgs",		optional_argument,	NULL, C_PKGS },
		{ "packages",		optional_argument,	NULL, C_PKGS },
		{ "pkgs-plist",		no_argument,		NULL, C_PKGS_PLIST },
		{ "groups",		no_argument,		NULL, C_GROUPS },
		{ "groups-plist",	no_argument,		NULL, C_GROUPS_PLIST },
		{ "group-pkgs",		required_argument,	NULL, C_GROUP_PKGS },
		{ "files",		required_argument,	NULL, C_FILES },
		{ "export-plist",	required_argument,	NULL, C_EXPORT },
		{ "pkg-info",		required_argument,	NULL, C_INFO },
		{ "pkg-info-plist",	required_argument,	NULL, C_INFO_PLIST },
		{ "pkg-groups",		required_argument,	NULL, C_PKG_GROUPS },
		{ "file-info",		required_argument,	NULL, C_FILE_INFO },
		{ "file-info-plist",	required_argument,	NULL,
		    C_FILE_INFO_PLIST },
		{ "forget",		required_argument,	NULL, C_FORGET },
		{ NULL, 0, NULL, 0 }
	};
	const char *arg = NULL;
	int ch, cmd = C_NONE, rc = 0, i;

	while ((ch = getopt_long(argc, argv, "vf", longopts, NULL)) != -1) {
		switch (ch) {
		case 'v':
			verbose = true;
			break;
		case 'f':
			force = true;
			break;
		case 'V':
			volume = optarg;
			break;
		case 'F':
			only_files = true;
			break;
		case 'D':
			only_dirs = true;
			break;
		case 'r':
			regexp_ids = true;
			break;
		case C_PKGS: case C_PKGS_PLIST: case C_GROUPS:
		case C_GROUPS_PLIST: case C_GROUP_PKGS: case C_FILES:
		case C_EXPORT: case C_INFO: case C_INFO_PLIST: case C_PKG_GROUPS:
		case C_FILE_INFO: case C_FILE_INFO_PLIST: case C_FORGET:
			if (cmd != C_NONE)
				usage();
			cmd = ch;
			arg = optarg;
			break;
		default:
			usage();
		}
	}
	if (cmd == C_NONE)
		usage();
	snprintf(receipts, sizeof(receipts), "%s%svar/db/receipts", volume,
	    volume[strlen(volume) - 1] == '/' ? "" : "/");

	switch (cmd) {
	case C_PKGS:
		return (pkgs(arg, false));
	case C_PKGS_PLIST:
		return (pkgs(NULL, true));
	case C_GROUPS:
		return (list_groups(false));
	case C_GROUPS_PLIST:
		return (list_groups(true));
	}
	/* The commands that take an argument take further ones too. */
	for (i = optind - 1; i < argc; i++) {
		const char *a = i < optind ? arg : argv[i];

		switch (cmd) {
		case C_GROUP_PKGS:	rc |= group_pkgs(a); break;
		case C_FILES:		rc |= files(a); break;
		case C_EXPORT:		rc |= export_plist(a); break;
		case C_INFO:		rc |= pkg_info(a, false); break;
		case C_INFO_PLIST:	rc |= pkg_info(a, true); break;
		case C_PKG_GROUPS:	rc |= pkg_groups(a); break;
		case C_FILE_INFO:	rc |= file_info(a, false); break;
		case C_FILE_INFO_PLIST:	rc |= file_info(a, true); break;
		case C_FORGET:		rc |= forget(a); break;
		}
	}
	(void)force;
	(void)verbose;
	return (rc);
}
