/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * mdfind: the files that match a metadata query, with the reference's
 * command's options and query language. There is no index yet (the
 * maintainer's choice, 2026-11-23): the files are searched on the disk,
 * under -onlyin's directories or, by default, the user's home folder and
 * the applications folders; packages (apps, bundles) are matched as one
 * file, not entered; invisible files are skipped.
 *
 * A query is either words, each of which must be in the file's name or,
 * for text files, its text (case and diacritics aside), or an expression:
 *
 *   attribute == 'value'[cdw]     (also !=; * in the value is a wildcard;
 *                                  c: any case, d: any diacritics, w: a word)
 *   attribute < > <= >= value     numbers, or dates: $time.now, $time.today,
 *                                  $time.today(-1), $time.iso(2026-11-23...)
 *   e && e, e || e, !e, (e)
 *
 * Attributes are CoreServices' MDItem's (kMDItemFSName, kMDItemContentType,
 * kMDItemContentTypeTree, kMDItemFSSize, ...) and kMDItemTextContent.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <ctype.h>
#include <err.h>
#include <fnmatch.h>
#include <fts.h>
#include <limits.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#include "mdvalue.h"

#define kTextLimit	(4 * 1024 * 1024)

enum { kWords, kCompare, kAnd, kOr, kNot };
enum { kEq, kNe, kLt, kGt, kLe, kGe };

typedef struct Node {
	int kind;
	/* kCompare */
	char *attribute;
	int op;
	char *value;
	int caseless, diacriticless, word;
	/* kWords */
	char **words;
	int wordCount;
	struct Node *left, *right;
} Node;

static const char *input;

static void
usage(void)
{
	fprintf(stderr, "Usage: mdfind [-live] [-count] [-onlyin directory] [-name fileName | -s smartFolderName | query]\n"
	    "list the files matching the query\n"
	    "query can be an expression or a sequence of words\n\n"
	    "\t-attr <attr>      Fetches the value of the specified attribute\n"
	    "\t-count            Query only reports matching items count\n"
	    "\t-onlyin <dir>     Search only within given directory\n"
	    "\t-live             Query should stay active\n"
	    "\t-name <name>      Search on file name only\n"
	    "\t-0                Use NUL (``\\0'') as a path separator, for use with xargs -0.\n\n"
	    "example:  mdfind image\n"
	    "example:  mdfind -onlyin ~ image\n"
	    "example:  mdfind -name stdlib.h\n"
	    "example:  mdfind \"kMDItemAuthor == '*MyFavoriteAuthor*'\"\n");
	exit(1);
}

/* ---- The query ---- */

static void
skipSpace(void)
{
	while (isspace((unsigned char)*input))
		input++;
}

static Node *parseOr(void);

static Node *
newNode(int kind)
{
	Node *node = calloc(1, sizeof(*node));

	if (node == NULL)
		err(1, NULL);
	node->kind = kind;
	return node;
}

static char *
parseToken(void)
{
	const char *start = input;

	while (*input != '\0' && !isspace((unsigned char)*input) && strchr("=!<>()&|'\"", *input) == NULL)
		input++;
	return strndup(start, (size_t)(input - start));
}

static Node *
parseComparison(void)
{
	Node *node = newNode(kCompare);

	skipSpace();
	node->attribute = parseToken();
	if (node->attribute[0] == '\0')
		errx(1, "Failed to create query for '%s'.", input);
	skipSpace();
	if (strncmp(input, "==", 2) == 0) node->op = kEq, input += 2;
	else if (strncmp(input, "!=", 2) == 0) node->op = kNe, input += 2;
	else if (strncmp(input, "<=", 2) == 0) node->op = kLe, input += 2;
	else if (strncmp(input, ">=", 2) == 0) node->op = kGe, input += 2;
	else if (*input == '<') node->op = kLt, input++;
	else if (*input == '>') node->op = kGt, input++;
	else if (*input == '=') node->op = kEq, input++;
	else
		errx(1, "Failed to create query: an operator is missing after %s.", node->attribute);
	skipSpace();
	if (*input == '\'' || *input == '"') {
		char quote = *input++;
		const char *start = input;

		while (*input != '\0' && *input != quote)
			input++;
		node->value = strndup(start, (size_t)(input - start));
		if (*input == quote)
			input++;
		while (*input == 'c' || *input == 'd' || *input == 'w') {
			if (*input == 'c') node->caseless = 1;
			if (*input == 'd') node->diacriticless = 1;
			if (*input == 'w') node->word = 1;
			input++;
		}
	} else {
		const char *start = input;
		int depth = 0;

		/* A bare value: a number or a $time function with its argument. */
		while (*input != '\0' && (depth > 0 || (!isspace((unsigned char)*input) && strchr("&|)", *input) == NULL))) {
			if (*input == '(') depth++;
			if (*input == ')') depth--;
			input++;
		}
		node->value = strndup(start, (size_t)(input - start));
	}
	return node;
}

static Node *
parseUnary(void)
{
	skipSpace();
	if (*input == '!' && input[1] != '=') {
		Node *node = newNode(kNot);
		input++;
		node->left = parseUnary();
		return node;
	}
	if (*input == '(') {
		Node *node;
		input++;
		node = parseOr();
		skipSpace();
		if (*input == ')')
			input++;
		return node;
	}
	return parseComparison();
}

static Node *
parseAnd(void)
{
	Node *left = parseUnary();

	for (;;) {
		skipSpace();
		if (strncmp(input, "&&", 2) != 0)
			return left;
		input += 2;
		Node *node = newNode(kAnd);
		node->left = left;
		node->right = parseUnary();
		left = node;
	}
}

static Node *
parseOr(void)
{
	Node *left = parseAnd();

	for (;;) {
		skipSpace();
		if (strncmp(input, "||", 2) != 0)
			return left;
		input += 2;
		Node *node = newNode(kOr);
		node->left = left;
		node->right = parseAnd();
		left = node;
	}
}

/* An expression has an operator; else the query is words. */
static Node *
parseQuery(const char *query)
{
	if (strstr(query, "==") || strstr(query, "!=") || strchr(query, '<') || strchr(query, '>') ||
	    (strstr(query, "kMDItem") && strchr(query, '='))) {
		input = query;
		Node *node = parseOr();
		skipSpace();
		if (*input != '\0')
			errx(1, "Failed to create query for '%s'.", query);
		return node;
	}

	Node *node = newNode(kWords);
	char *copy = strdup(query), *word, *rest = copy;

	while ((word = strsep(&rest, " \t")) != NULL)
		if (*word != '\0') {
			node->words = realloc(node->words, sizeof(char *) * (size_t)(node->wordCount + 1));
			node->words[node->wordCount++] = strdup(word);
		}
	free(copy);
	if (node->wordCount == 0)
		usage();
	return node;
}

/* ---- Matching ---- */

/* Folded for comparison: lower case, and without diacritics when asked. */
static CFStringRef
fold(CFStringRef string, int caseless, int diacriticless)
{
	CFMutableStringRef folded = CFStringCreateMutableCopy(NULL, 0, string);
	CFOptionFlags flags = (caseless ? kCFCompareCaseInsensitive : 0) |
	    (diacriticless ? kCFCompareDiacriticInsensitive : 0);

	if (flags != 0)
		CFStringFold(folded, flags, NULL);
	return folded;
}

static int
matchString(CFStringRef actual, const Node *node)
{
	char a[4096], v[4096];
	CFStringRef wanted = CFStringCreateWithCString(NULL, node->value, kCFStringEncodingUTF8);
	CFStringRef fa = fold(actual, node->caseless, node->diacriticless);
	CFStringRef fv = fold(wanted, node->caseless, node->diacriticless);
	int result;

	md_cstring(fa, a, sizeof(a));
	md_cstring(fv, v, sizeof(v));
	if (node->word) {
		/* A word of it begins as the value. */
		result = 0;
		for (char *p = a; *p != '\0' && !result; p++)
			if ((p == a || !isalnum((unsigned char)p[-1])) && fnmatch(v, p, 0) == 0)
				result = 1;
		if (!result) {
			char pattern[4200];
			snprintf(pattern, sizeof(pattern), "%s*", v);
			for (char *p = a; *p != '\0' && !result; p++)
				if ((p == a || !isalnum((unsigned char)p[-1])) && fnmatch(pattern, p, 0) == 0)
					result = 1;
		}
	} else
		result = fnmatch(v, a, 0) == 0;
	CFRelease(wanted);
	CFRelease(fa);
	CFRelease(fv);
	return result;
}

/* A $time value, or a number, in seconds since 1970. */
static int
parseTime(const char *value, double *seconds)
{
	time_t now = time(NULL);
	struct tm tm;
	char *end;

	if (strcmp(value, "$time.now") == 0) {
		*seconds = (double)now;
		return 1;
	}
	if (strncmp(value, "$time.now(", 10) == 0) {
		*seconds = (double)now + strtod(value + 10, NULL);
		return 1;
	}
	if (strncmp(value, "$time.today", 11) == 0) {
		localtime_r(&now, &tm);
		tm.tm_hour = tm.tm_min = tm.tm_sec = 0;
		if (value[11] == '(')
			tm.tm_mday += (int)strtol(value + 12, NULL, 10);
		*seconds = (double)mktime(&tm);
		return 1;
	}
	if (strncmp(value, "$time.iso(", 10) == 0) {
		memset(&tm, 0, sizeof(tm));
		if (strptime(value + 10, "%Y-%m-%dT%H:%M:%S", &tm) == NULL &&
		    strptime(value + 10, "%Y-%m-%d %H:%M:%S", &tm) == NULL &&
		    strptime(value + 10, "%Y-%m-%d", &tm) == NULL)
			return 0;
		*seconds = (double)timegm(&tm);
		return 1;
	}
	*seconds = strtod(value, &end);
	return end != value && *end == '\0';
}

static int
compareNumbers(double a, double b, int op)
{
	switch (op) {
	case kEq: return a == b;
	case kNe: return a != b;
	case kLt: return a < b;
	case kGt: return a > b;
	case kLe: return a <= b;
	default: return a >= b;
	}
}

static int
matchValue(CFTypeRef actual, const Node *node)
{
	CFTypeID type;
	double wanted;

	if (actual == NULL)
		return node->op == kNe;
	type = CFGetTypeID(actual);
	if (type == CFArrayGetTypeID()) {
		/* Any of its items (none, for !=). */
		int any = 0;
		Node equal = *node;
		if (node->op == kNe)
			equal.op = kEq;
		for (CFIndex i = 0; i < CFArrayGetCount(actual) && !any; i++)
			any = matchValue(CFArrayGetValueAtIndex(actual, i), &equal);
		return node->op == kNe ? !any : any;
	}
	if (type == CFStringGetTypeID()) {
		if (node->op == kEq || node->op == kNe) {
			int match = matchString(actual, node);
			return node->op == kEq ? match : !match;
		}
		char a[1024];
		int order = strcmp(md_cstring(actual, a, sizeof(a)), node->value);
		return compareNumbers(order, 0, node->op);
	}
	if (type == CFBooleanGetTypeID())
		return compareNumbers(CFBooleanGetValue(actual) ? 1 : 0, strtod(node->value, NULL), node->op);
	if (!parseTime(node->value, &wanted))
		return 0;
	if (type == CFNumberGetTypeID()) {
		double n = 0;
		CFNumberGetValue(actual, kCFNumberDoubleType, &n);
		return compareNumbers(n, wanted, node->op);
	}
	if (type == CFDateGetTypeID())
		return compareNumbers(CFDateGetAbsoluteTime(actual) + kCFAbsoluteTimeIntervalSince1970, wanted, node->op);
	return 0;
}

/* A text file's text (its first megabytes). */
static CFStringRef
copyTextContent(MDItemRef item, const char *path)
{
	CFStringRef type = MDItemCopyAttribute(item, kMDItemContentType);
	CFStringRef text = NULL;
	FILE *file;
	struct stat st;

	if (type == NULL)
		return NULL;
	if (UTTypeConformsTo(type, CFSTR("public.text")) && stat(path, &st) == 0 && S_ISREG(st.st_mode) &&
	    (file = fopen(path, "r")) != NULL) {
		size_t size = st.st_size > kTextLimit ? kTextLimit : (size_t)st.st_size;
		char *bytes = malloc(size ? size : 1);
		size_t got = bytes != NULL ? fread(bytes, 1, size, file) : 0;

		fclose(file);
		if (bytes != NULL) {
			text = CFStringCreateWithBytes(NULL, (const UInt8 *)bytes, (CFIndex)got, kCFStringEncodingUTF8, false);
			if (text == NULL)
				text = CFStringCreateWithBytes(NULL, (const UInt8 *)bytes, (CFIndex)got, kCFStringEncodingISOLatin1, false);
			free(bytes);
		}
	}
	CFRelease(type);
	return text;
}

static CFTypeRef
copyAttribute(MDItemRef item, const char *path, const char *attribute)
{
	if (strcmp(attribute, "kMDItemTextContent") == 0)
		return copyTextContent(item, path);
	if (strcmp(attribute, "*") == 0)
		return MDItemCopyAttribute(item, kMDItemDisplayName);
	CFStringRef name = CFStringCreateWithCString(NULL, attribute, kCFStringEncodingUTF8);
	CFTypeRef value = MDItemCopyAttribute(item, name);
	CFRelease(name);
	return value;
}

static int
contains(CFStringRef haystack, const char *word)
{
	CFStringRef needle = CFStringCreateWithCString(NULL, word, kCFStringEncodingUTF8);
	CFRange found = CFStringFind(haystack, needle, kCFCompareCaseInsensitive | kCFCompareDiacriticInsensitive);

	CFRelease(needle);
	return found.location != kCFNotFound;
}

static int
matches(const Node *node, MDItemRef item, const char *path)
{
	switch (node->kind) {
	case kAnd:
		return matches(node->left, item, path) && matches(node->right, item, path);
	case kOr:
		return matches(node->left, item, path) || matches(node->right, item, path);
	case kNot:
		return !matches(node->left, item, path);
	case kWords: {
		CFStringRef name = MDItemCopyAttribute(item, kMDItemDisplayName);
		CFStringRef text = NULL;
		int all = 1, textRead = 0;

		for (int i = 0; i < node->wordCount && all; i++) {
			if (name != NULL && contains(name, node->words[i]))
				continue;
			if (!textRead) {
				text = copyTextContent(item, path);
				textRead = 1;
			}
			all = text != NULL && contains(text, node->words[i]);
		}
		if (name != NULL)
			CFRelease(name);
		if (text != NULL)
			CFRelease(text);
		return all;
	}
	default: {
		CFTypeRef value = copyAttribute(item, path, node->attribute);
		int result = matchValue(value, node);
		if (value != NULL)
			CFRelease(value);
		return result;
	}
	}
}

/* ---- The search ---- */

static int countOnly, nulSeparated;
static const char *shownAttribute;
static long found;

static void
report(MDItemRef item, const char *path)
{
	found++;
	if (countOnly)
		return;
	fputs(path, stdout);
	if (shownAttribute != NULL) {
		CFTypeRef value = copyAttribute(item, path, shownAttribute);
		printf("   %s = ", shownAttribute);
		md_print_value(stdout, value, 0, 0);
		if (value != NULL)
			CFRelease(value);
	}
	putchar(nulSeparated ? '\0' : '\n');
}

static void
search(char *const *roots, const Node *query)
{
	FTS *fts = fts_open(roots, FTS_PHYSICAL | FTS_NOCHDIR, NULL);
	FTSENT *entry;

	if (fts == NULL)
		return;
	while ((entry = fts_read(fts)) != NULL) {
		if (entry->fts_info == FTS_DP || entry->fts_info == FTS_DNR || entry->fts_info == FTS_ERR ||
		    entry->fts_info == FTS_NS)
			continue;
		/* Invisible files are not searched; a root is, whatever its name. */
		if (entry->fts_level > 0 && entry->fts_name[0] == '.') {
			if (entry->fts_info == FTS_D)
				fts_set(fts, entry, FTS_SKIP);
			continue;
		}
		CFStringRef path = CFStringCreateWithFileSystemRepresentation(NULL, entry->fts_path);
		MDItemRef item = path != NULL ? MDItemCreate(NULL, path) : NULL;

		if (path != NULL)
			CFRelease(path);
		if (item == NULL)
			continue;
		if (entry->fts_level > 0 && matches(query, item, entry->fts_path))
			report(item, entry->fts_path);
		/* A package is one file: not entered. */
		if (entry->fts_info == FTS_D && entry->fts_level > 0) {
			CFStringRef type = MDItemCopyAttribute(item, kMDItemContentType);
			if (type != NULL) {
				if (UTTypeConformsTo(type, CFSTR("com.apple.package")) || UTTypeConformsTo(type, CFSTR("com.apple.bundle")))
					fts_set(fts, entry, FTS_SKIP);
				CFRelease(type);
			}
		}
		CFRelease(item);
	}
	fts_close(fts);
}

int
main(int argc, char *argv[])
{
	char *roots[64];
	int rootCount = 0, i;
	const char *name = NULL;
	char *query = NULL;

	for (i = 1; i < argc; i++) {
		if (strcmp(argv[i], "-onlyin") == 0 && i + 1 < argc) {
			char resolved[PATH_MAX];
			if (rootCount < 63)
				roots[rootCount++] = strdup(realpath(argv[i + 1], resolved) != NULL ? resolved : argv[i + 1]);
			i++;
		} else if (strcmp(argv[i], "-name") == 0 && i + 1 < argc)
			name = argv[++i];
		else if (strcmp(argv[i], "-count") == 0)
			countOnly = 1;
		else if (strcmp(argv[i], "-0") == 0)
			nulSeparated = 1;
		else if (strcmp(argv[i], "-attr") == 0 && i + 1 < argc)
			shownAttribute = argv[++i];
		else if (strcmp(argv[i], "-live") == 0 || strcmp(argv[i], "-s") == 0)
			errx(1, "%s needs the metadata index, which this system does not have yet", argv[i]);
		else if (strcmp(argv[i], "-reprint") == 0)
			;
		else if (argv[i][0] == '-' && argv[i][1] != '\0')
			usage();
		else {
			/* The rest is the query, words joined. */
			size_t length = 0;
			for (int k = i; k < argc; k++)
				length += strlen(argv[k]) + 1;
			query = calloc(1, length + 1);
			for (int k = i; k < argc; k++) {
				strcat(query, argv[k]);
				if (k + 1 < argc)
					strcat(query, " ");
			}
			break;
		}
	}
	if (name == NULL && query == NULL) {
		fprintf(stderr, "mdfind: no query specified.\n\n");
		usage();
	}

	Node *node;
	if (name != NULL) {
		char expression[PATH_MAX + 64];
		snprintf(expression, sizeof(expression), "kMDItemFSName == '*%s*'cd", name);
		node = parseQuery(expression);
	} else
		node = parseQuery(query);

	if (rootCount == 0) {
		const char *home = getenv("HOME");
		struct passwd *pw = getpwuid(getuid());

		if (home == NULL && pw != NULL)
			home = pw->pw_dir;
		if (home != NULL)
			roots[rootCount++] = strdup(home);
		roots[rootCount++] = "/Applications";
		roots[rootCount++] = "/System/Applications";
	}
	roots[rootCount] = NULL;
	search(roots, node);
	if (countOnly)
		printf("%ld\n", found);
	return 0;
}
