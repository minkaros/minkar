/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * launchservicesd's database (LaunchServices interface version 3). The
 * applications are the bundles in a few folders (and any registered);
 * each one's Info.plist gives its identifier, its document types
 * (CFBundleDocumentTypes: LSItemContentTypes, CFBundleTypeExtensions,
 * CFBundleTypeRole, LSHandlerRank) and the types it declares
 * (UTExportedTypeDeclarations, UTImportedTypeDeclarations). The folders
 * are read again when their modification time changes; an application
 * changed in place is read again when registered (LSRegisterURL).
 *
 * A file's type comes from its extension, else from what it is (a
 * folder, text, an executable). The applications that open it are those
 * whose document types name that type or one it conforms to (the closer
 * the better), then by rank, then by folder.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include "LSDatabase.h"

/* The roles (LSRolesMask) and ranks. */
enum { kRoleNone = 1, kRoleViewer = 2, kRoleEditor = 4, kRoleShell = 8 };
enum { kRankNone = 0, kRankAlternate = 1, kRankDefault = 2, kRankOwner = 3 };

#define MAX_LIST	32	/* types or extensions in one declaration */

/* A type identifier: what it conforms to and its extensions. */
typedef struct {
	char	*uti;
	char	*conforms[MAX_LIST];
	char	*extensions[MAX_LIST];
} Type;

typedef struct {
	unsigned roles;
	int	rank;
	char	*types[MAX_LIST];
	char	*extensions[MAX_LIST];
} DocType;

typedef struct {
	char	path[1024];
	char	*bundleID;
	int	folder;		/* the order of its folder: lower is preferred */
	DocType	*docTypes;
	unsigned docTypeCount;
	Type	*declared;	/* exported, then imported */
	unsigned declaredCount;
} App;

/* The folders, in the order their applications are preferred. */
static const char *const kFolders[] = {
	"/Applications",
	"/Applications/Utilities",
	"/System/Applications",
	"/System/Applications/Utilities",
	"/System/Library/CoreServices",
};
#define FOLDER_COUNT	(sizeof(kFolders) / sizeof(kFolders[0]))
#define REGISTERED	FOLDER_COUNT	/* the folder order of one registered */

static App *apps;
static unsigned appCount;
static struct timespec folderTimes[FOLDER_COUNT];
static int loaded;
/* Applications registered from elsewhere: kept across rescans. */
static char **registered;
static unsigned registeredCount;

/* The types it knows by itself: identifier, what it conforms to, its
 * extensions. Applications may declare more. */
static const struct { const char *uti, *conforms, *extensions; } kTypes[] = {
	{ "public.item", "", "" },
	{ "public.content", "", "" },
	{ "public.data", "public.item", "" },
	{ "public.directory", "public.item", "" },
	{ "public.folder", "public.directory", "" },
	{ "com.apple.package", "public.directory", "" },
	{ "com.apple.bundle", "public.directory", "" },
	{ "com.apple.application", "public.data", "" },
	{ "com.apple.application-bundle", "com.apple.application com.apple.bundle com.apple.package", "app" },
	{ "public.executable", "public.item", "" },
	{ "public.unix-executable", "public.data public.executable", "" },
	{ "public.text", "public.data public.content", "" },
	{ "public.plain-text", "public.text", "txt text" },
	{ "public.utf8-plain-text", "public.plain-text", "" },
	{ "public.source-code", "public.plain-text", "" },
	{ "public.c-source", "public.source-code", "c" },
	{ "public.c-header", "public.source-code", "h" },
	{ "public.objective-c-source", "public.source-code", "m" },
	{ "public.c-plus-plus-source", "public.source-code", "cpp cc cxx c++" },
	{ "public.c-plus-plus-header", "public.source-code", "hpp hh hxx" },
	{ "public.objective-c-plus-plus-source", "public.source-code", "mm" },
	{ "public.swift-source", "public.source-code", "swift" },
	{ "public.assembly-source", "public.source-code", "s" },
	{ "public.script", "public.source-code", "" },
	{ "public.shell-script", "public.script", "sh bash zsh csh ksh command" },
	{ "public.python-script", "public.script", "py" },
	{ "public.perl-script", "public.script", "pl pm" },
	{ "public.ruby-script", "public.script", "rb" },
	{ "com.netscape.javascript-source", "public.script", "js mjs" },
	{ "public.log", "public.plain-text", "log" },
	{ "public.comma-separated-values-text", "public.plain-text", "csv" },
	{ "public.tab-separated-values-text", "public.plain-text", "tsv" },
	{ "public.json", "public.text", "json" },
	{ "public.yaml", "public.plain-text", "yaml yml" },
	{ "net.daringfireball.markdown", "public.plain-text", "md markdown" },
	{ "public.xml", "public.text", "xml" },
	{ "com.apple.property-list", "public.data", "plist" },
	{ "public.html", "public.text", "html htm" },
	{ "public.css", "public.plain-text", "css" },
	{ "public.rtf", "public.text", "rtf" },
	{ "public.image", "public.data public.content", "" },
	{ "public.png", "public.image", "png" },
	{ "public.jpeg", "public.image", "jpg jpeg" },
	{ "com.compuserve.gif", "public.image", "gif" },
	{ "public.tiff", "public.image", "tif tiff" },
	{ "com.microsoft.bmp", "public.image", "bmp" },
	{ "org.webmproject.webp", "public.image", "webp" },
	{ "public.svg-image", "public.image public.xml", "svg" },
	{ "com.adobe.pdf", "public.data public.content", "pdf" },
	{ "public.audio", "public.data public.content", "" },
	{ "public.movie", "public.data public.content", "" },
	{ "public.archive", "public.data", "" },
	{ "public.zip-archive", "public.archive", "zip" },
	{ "org.gnu.gnu-zip-archive", "public.archive", "gz tgz" },
	{ "public.tar-archive", "public.archive", "tar" },
	{ "public.bzip2-archive", "public.archive", "bz2" },
	{ "org.tukaani.xz-archive", "public.archive", "xz" },
	{ "com.apple.disk-image", "public.data", "dmg" },
};
#define TYPE_COUNT	(sizeof(kTypes) / sizeof(kTypes[0]))

static Type builtin[TYPE_COUNT];

/* Space-separated words into a NULL-ended list. */
static void
splitWords(const char *words, char **list)
{
	char *copy = strdup(words), *word, *rest = copy;
	unsigned n = 0;

	while ((word = strsep(&rest, " ")) != NULL)
		if (*word != '\0' && n < MAX_LIST - 1)
			list[n++] = strdup(word);
	list[n] = NULL;
	free(copy);
}

static void
loadBuiltin(void)
{
	for (unsigned i = 0; i < TYPE_COUNT; i++) {
		builtin[i].uti = (char *)kTypes[i].uti;
		splitWords(kTypes[i].conforms, builtin[i].conforms);
		splitWords(kTypes[i].extensions, builtin[i].extensions);
	}
}

/* -- Info.plist -- */

static char *
copyCString(CFTypeRef value)
{
	char buffer[1024];

	if (value == NULL || CFGetTypeID(value) != CFStringGetTypeID() ||
	    !CFStringGetCString(value, buffer, sizeof(buffer), kCFStringEncodingUTF8))
		return NULL;
	return strdup(buffer);
}

/* A string or an array of strings into a NULL-ended list (lower case
 * when asked: extensions). */
static void
copyList(CFTypeRef value, char **list, int lower)
{
	unsigned n = 0;

	list[0] = NULL;
	if (value == NULL)
		return;
	if (CFGetTypeID(value) == CFStringGetTypeID()) {
		list[n] = copyCString(value);
		if (list[n] != NULL)
			n++;
	} else if (CFGetTypeID(value) == CFArrayGetTypeID()) {
		CFIndex count = CFArrayGetCount(value);
		for (CFIndex i = 0; i < count && n < MAX_LIST - 1; i++) {
			list[n] = copyCString(CFArrayGetValueAtIndex(value, i));
			if (list[n] != NULL)
				n++;
		}
	}
	list[n] = NULL;
	for (unsigned i = 0; lower && i < n; i++)
		for (char *c = list[i]; *c; c++)
			if (*c >= 'A' && *c <= 'Z')
				*c += 'a' - 'A';
}

static void
freeList(char **list)
{
	for (unsigned i = 0; list[i] != NULL; i++)
		free(list[i]);
}

static CFDictionaryRef
copyInfo(const char *bundle)
{
	char path[1100];
	struct stat st;
	FILE *f;

	snprintf(path, sizeof(path), "%s/Contents/Info.plist", bundle);
	if (stat(path, &st) != 0 || st.st_size <= 0 || st.st_size > 4 * 1024 * 1024 ||
	    (f = fopen(path, "r")) == NULL)
		return NULL;
	UInt8 *bytes = malloc((size_t)st.st_size);
	size_t n = bytes ? fread(bytes, 1, (size_t)st.st_size, f) : 0;
	fclose(f);
	if (n != (size_t)st.st_size) {
		free(bytes);
		return NULL;
	}
	CFDataRef data = CFDataCreate(NULL, bytes, (CFIndex)n);
	free(bytes);
	CFPropertyListRef plist = data ? CFPropertyListCreateWithData(NULL, data,
	    kCFPropertyListImmutable, NULL, NULL) : NULL;
	if (data != NULL)
		CFRelease(data);
	if (plist != NULL && CFGetTypeID(plist) != CFDictionaryGetTypeID()) {
		CFRelease(plist);
		plist = NULL;
	}
	return plist;
}

static unsigned
rolesFor(CFTypeRef value)
{
	char *role = copyCString(value);
	unsigned roles = kRoleViewer;

	if (role != NULL) {
		if (strcmp(role, "Editor") == 0)
			roles = kRoleEditor | kRoleViewer;	/* an editor views too */
		else if (strcmp(role, "Shell") == 0)
			roles = kRoleShell;
		else if (strcmp(role, "None") == 0)
			roles = kRoleNone;
		free(role);
	}
	return roles;
}

static int
rankFor(CFTypeRef value)
{
	char *rank = copyCString(value);
	int r = kRankDefault;

	if (rank != NULL) {
		if (strcmp(rank, "Owner") == 0)
			r = kRankOwner;
		else if (strcmp(rank, "Alternate") == 0)
			r = kRankAlternate;
		else if (strcmp(rank, "None") == 0)
			r = kRankNone;
		free(rank);
	}
	return r;
}

static void
readDeclarations(App *app, CFTypeRef list)
{
	if (list == NULL || CFGetTypeID(list) != CFArrayGetTypeID())
		return;
	CFIndex count = CFArrayGetCount(list);
	app->declared = realloc(app->declared, (app->declaredCount + (unsigned)count) * sizeof(Type));
	for (CFIndex i = 0; i < count; i++) {
		CFDictionaryRef d = CFArrayGetValueAtIndex(list, i);
		if (CFGetTypeID(d) != CFDictionaryGetTypeID())
			continue;
		char *uti = copyCString(CFDictionaryGetValue(d, CFSTR("UTTypeIdentifier")));
		if (uti == NULL)
			continue;
		Type *t = &app->declared[app->declaredCount++];
		t->uti = uti;
		copyList(CFDictionaryGetValue(d, CFSTR("UTTypeConformsTo")), t->conforms, 0);
		CFDictionaryRef tags = CFDictionaryGetValue(d, CFSTR("UTTypeTagSpecification"));
		copyList(tags && CFGetTypeID(tags) == CFDictionaryGetTypeID() ?
		    CFDictionaryGetValue(tags, CFSTR("public.filename-extension")) : NULL,
		    t->extensions, 1);
	}
}

/* An application bundle into a record; 0 when it is one. */
static int
readApp(App *app, const char *path, int folder)
{
	CFDictionaryRef info = copyInfo(path);

	memset(app, 0, sizeof(*app));
	if (info == NULL)
		return -1;
	strlcpy(app->path, path, sizeof(app->path));
	app->folder = folder;
	app->bundleID = copyCString(CFDictionaryGetValue(info, CFSTR("CFBundleIdentifier")));
	CFArrayRef docs = CFDictionaryGetValue(info, CFSTR("CFBundleDocumentTypes"));
	if (docs != NULL && CFGetTypeID(docs) == CFArrayGetTypeID()) {
		CFIndex count = CFArrayGetCount(docs);
		app->docTypes = calloc((size_t)count ? (size_t)count : 1, sizeof(DocType));
		for (CFIndex i = 0; i < count; i++) {
			CFDictionaryRef d = CFArrayGetValueAtIndex(docs, i);
			if (CFGetTypeID(d) != CFDictionaryGetTypeID())
				continue;
			DocType *t = &app->docTypes[app->docTypeCount++];
			t->roles = rolesFor(CFDictionaryGetValue(d, CFSTR("CFBundleTypeRole")));
			t->rank = rankFor(CFDictionaryGetValue(d, CFSTR("LSHandlerRank")));
			copyList(CFDictionaryGetValue(d, CFSTR("LSItemContentTypes")), t->types, 0);
			copyList(CFDictionaryGetValue(d, CFSTR("CFBundleTypeExtensions")), t->extensions, 1);
		}
	}
	readDeclarations(app, CFDictionaryGetValue(info, CFSTR("UTExportedTypeDeclarations")));
	readDeclarations(app, CFDictionaryGetValue(info, CFSTR("UTImportedTypeDeclarations")));
	CFRelease(info);
	return 0;
}

static void
freeApp(App *app)
{
	free(app->bundleID);
	for (unsigned i = 0; i < app->docTypeCount; i++) {
		freeList(app->docTypes[i].types);
		freeList(app->docTypes[i].extensions);
	}
	free(app->docTypes);
	for (unsigned i = 0; i < app->declaredCount; i++) {
		free(app->declared[i].uti);
		freeList(app->declared[i].conforms);
		freeList(app->declared[i].extensions);
	}
	free(app->declared);
}

static int
hasSuffix(const char *s, const char *suffix)
{
	size_t n = strlen(s), m = strlen(suffix);

	return n > m && strcasecmp(s + n - m, suffix) == 0;
}

static void
addApp(const char *path, int folder)
{
	App app;

	for (unsigned i = 0; i < appCount; i++)
		if (strcmp(apps[i].path, path) == 0)
			return;
	if (readApp(&app, path, folder) != 0)
		return;
	apps = realloc(apps, (appCount + 1) * sizeof(App));
	apps[appCount++] = app;
}

static void
scan(void)
{
	for (unsigned i = 0; i < appCount; i++)
		freeApp(&apps[i]);
	appCount = 0;
	for (unsigned f = 0; f < FOLDER_COUNT; f++) {
		struct stat st;
		DIR *dir;
		struct dirent *e;

		memset(&folderTimes[f], 0, sizeof(folderTimes[f]));
		if (stat(kFolders[f], &st) == 0)
			folderTimes[f] = st.st_mtimespec;
		if ((dir = opendir(kFolders[f])) == NULL)
			continue;
		while ((e = readdir(dir)) != NULL) {
			char path[1024];
			if (e->d_name[0] == '.' || !hasSuffix(e->d_name, ".app"))
				continue;
			snprintf(path, sizeof(path), "%s/%s", kFolders[f], e->d_name);
			addApp(path, (int)f);
		}
		closedir(dir);
	}
	for (unsigned i = 0; i < registeredCount; i++)
		addApp(registered[i], REGISTERED);
	loaded = 1;
	fprintf(stderr, "launchservicesd: %u applications\n", appCount);
}

void
LSDBRefresh(void)
{
	static int builtinLoaded;

	if (!builtinLoaded) {
		loadBuiltin();
		builtinLoaded = 1;
	}
	for (unsigned f = 0; loaded && f < FOLDER_COUNT; f++) {
		struct stat st;
		struct timespec t = { 0, 0 };
		if (stat(kFolders[f], &st) == 0)
			t = st.st_mtimespec;
		if (t.tv_sec != folderTimes[f].tv_sec || t.tv_nsec != folderTimes[f].tv_nsec) {
			loaded = 0;
			break;
		}
	}
	if (!loaded)
		scan();
}

/* -- Types -- */

/* A type's declaration: the built-in one, else the first application's. */
static const Type *
findType(const char *uti)
{
	for (unsigned i = 0; i < TYPE_COUNT; i++)
		if (strcmp(builtin[i].uti, uti) == 0)
			return &builtin[i];
	for (unsigned a = 0; a < appCount; a++)
		for (unsigned i = 0; i < apps[a].declaredCount; i++)
			if (strcmp(apps[a].declared[i].uti, uti) == 0)
				return &apps[a].declared[i];
	return NULL;
}

static int
listHas(char *const *list, const char *word)
{
	for (unsigned i = 0; list[i] != NULL; i++)
		if (strcmp(list[i], word) == 0)
			return 1;
	return 0;
}

static const char *
typeForExtension(const char *extension)
{
	if (*extension == '\0')
		return NULL;
	for (unsigned i = 0; i < TYPE_COUNT; i++)
		if (listHas(builtin[i].extensions, extension))
			return builtin[i].uti;
	for (unsigned a = 0; a < appCount; a++)
		for (unsigned i = 0; i < apps[a].declaredCount; i++)
			if (listHas(apps[a].declared[i].extensions, extension))
				return apps[a].declared[i].uti;
	return NULL;
}

static const char *
typeForFile(const LSSessionFile *file)
{
	const char *uti;

	if (file->flags & kLSSessionFileIsDirectory) {
		uti = typeForExtension(file->extension);
		return uti != NULL ? uti : "public.folder";
	}
	if ((uti = typeForExtension(file->extension)) != NULL)
		return uti;
	if (file->flags & kLSSessionFileIsText)
		return "public.plain-text";
	if (file->flags & kLSSessionFileIsExecutable)
		return "public.unix-executable";
	return "public.data";
}

/* What a type conforms to, nearest first: chain[i] at distance[i]. */
#define MAX_CHAIN	48
static unsigned
conformance(const char *uti, const char **chain, int *distance)
{
	unsigned n = 0;

	chain[n] = uti;
	distance[n++] = 0;
	for (unsigned i = 0; i < n; i++) {
		const Type *t = findType(chain[i]);
		for (unsigned c = 0; t != NULL && t->conforms[c] != NULL && n < MAX_CHAIN; c++) {
			int seen = 0;
			for (unsigned j = 0; j < n && !seen; j++)
				seen = strcmp(chain[j], t->conforms[c]) == 0;
			if (!seen) {
				chain[n] = t->conforms[c];
				distance[n++] = distance[i] + 1;
			}
		}
	}
	return n;
}

/* -- Handlers -- */

typedef struct {
	unsigned app;
	int distance, rank;
} Candidate;

static int
better(const Candidate *a, const Candidate *b)
{
	if (a->distance != b->distance)
		return a->distance < b->distance;
	if (a->rank != b->rank)
		return a->rank > b->rank;
	if (apps[a->app].folder != apps[b->app].folder)
		return apps[a->app].folder < apps[b->app].folder;
	return strcmp(apps[a->app].path, apps[b->app].path) < 0;
}

unsigned
LSDBCopyHandlers(const LSSessionFile *file, char uti[128], LSSessionPath *out, unsigned max)
{
	const char *chain[MAX_CHAIN];
	int distance[MAX_CHAIN];
	Candidate found[64];
	unsigned n = 0, roles = file->roles ? file->roles : ~0u;
	char extension[sizeof(file->extension)];

	strlcpy(extension, file->extension, sizeof(extension));
	const char *type = typeForFile(file);
	strlcpy(uti, type, 128);
	unsigned links = conformance(type, chain, distance);

	for (unsigned a = 0; a < appCount && n < 64; a++) {
		Candidate best = { a, 1000, 0 };
		for (unsigned d = 0; d < apps[a].docTypeCount; d++) {
			DocType *t = &apps[a].docTypes[d];
			if (t->rank == kRankNone || (t->roles & roles) == 0)
				continue;
			Candidate c = { a, 1000, t->rank };
			for (unsigned i = 0; i < links; i++)
				if (listHas(t->types, chain[i]) && distance[i] < c.distance)
					c.distance = distance[i];
			/* An extension named outright is as close as it gets;
			 * "*" (any file) is the farthest. */
			if (extension[0] != '\0' && listHas(t->extensions, extension))
				c.distance = 0;
			else if (c.distance == 1000 && listHas(t->extensions, "*") &&
			    !(file->flags & kLSSessionFileIsDirectory))
				c.distance = 100;
			if (c.distance < 1000 && better(&c, &best))
				best = c;
		}
		if (best.distance < 1000)
			found[n++] = best;
	}
	/* Best first (a handful: insertion sort). */
	for (unsigned i = 1; i < n; i++)
		for (unsigned j = i; j > 0 && better(&found[j], &found[j - 1]); j--) {
			Candidate t = found[j];
			found[j] = found[j - 1];
			found[j - 1] = t;
		}
	if (n > max)
		n = max;
	for (unsigned i = 0; i < n; i++) {
		memset(&out[i], 0, sizeof(out[i]));
		strlcpy(out[i].path, apps[found[i].app].path, sizeof(out[i].path));
	}
	return n;
}

int
LSDBAppForBundleIdentifier(const char *bundleIdentifier, char path[1024])
{
	const App *best = NULL;

	for (unsigned a = 0; a < appCount; a++)
		if (apps[a].bundleID != NULL && strcasecmp(apps[a].bundleID, bundleIdentifier) == 0 &&
		    (best == NULL || apps[a].folder < best->folder))
			best = &apps[a];
	if (best == NULL)
		return -1;
	strlcpy(path, best->path, 1024);
	return 0;
}

int
LSDBRegister(const char *path)
{
	char real[1024];
	App app;

	if (realpath(path, real) == NULL || readApp(&app, real, REGISTERED) != 0)
		return -1;
	freeApp(&app);
	/* In one of the folders: read with them. Elsewhere: kept. */
	int inFolder = 0;
	for (unsigned f = 0; f < FOLDER_COUNT && !inFolder; f++) {
		size_t n = strlen(kFolders[f]);
		inFolder = strncmp(real, kFolders[f], n) == 0 && real[n] == '/' &&
		    strchr(real + n + 1, '/') == NULL;
	}
	if (!inFolder) {
		int known = 0;
		for (unsigned i = 0; i < registeredCount && !known; i++)
			known = strcmp(registered[i], real) == 0;
		if (!known) {
			registered = realloc(registered, (registeredCount + 1) * sizeof(char *));
			registered[registeredCount++] = strdup(real);
		}
	}
	scan();
	return 0;
}
