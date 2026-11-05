/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * open: opens files, folders and applications as a double click in the file manager
 * does, through LaunchServices. -a and -b name the application, -e and
 * -t open with the text editor, -R shows the items in the file manager instead, -n
 * starts a new instance of the application.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <CoreServices/CoreServices.h>
#include <err.h>
#include <getopt.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEXT_EDITOR_ID	"org.minkar.TextEditor"

static void
usage(void)
{
	fprintf(stderr,
	    "usage: open [-e | -t | -a application | -b bundle-id] [-R] [-n] file ...\n"
	    "       open -a application\n"
	    "       open -b bundle-id\n"
	    "  -a application  open with this application (a name or a path)\n"
	    "  -b bundle-id    open with the application with this identifier\n"
	    "  -e, -t          open with the text editor\n"
	    "  -R, --reveal    show the files in the file manager instead of opening them\n"
	    "  -n              open a new instance of the application\n"
	    "  -h, --help      show this help\n");
	exit(1);
}

static CFURLRef
fileURL(const char *path, Boolean isDirectory)
{
	return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
	    (CFIndex)strlen(path), isDirectory);
}

/* An application by name ("TextEditor", "TextEditor.app") or path. */
static CFURLRef
applicationNamed(const char *name)
{
	static const char *const folders[] = {
		"/Applications", "/Applications/Utilities", "/System/Applications",
		"/System/Applications/Utilities", "/System/Library/CoreServices",
	};
	char path[PATH_MAX];
	struct stat st;

	if (strchr(name, '/') != NULL) {
		if (realpath(name, path) == NULL || stat(path, &st) != 0)
			return NULL;
		return fileURL(path, S_ISDIR(st.st_mode));
	}
	for (size_t i = 0; i < sizeof(folders) / sizeof(folders[0]); i++) {
		size_t n = strlen(name);
		snprintf(path, sizeof(path), "%s/%s%s", folders[i], name,
		    n > 4 && strcasecmp(name + n - 4, ".app") == 0 ? "" : ".app");
		if (stat(path, &st) == 0)
			return fileURL(path, true);
	}
	return NULL;
}

static CFURLRef
applicationWithIdentifier(const char *bundleID)
{
	CFStringRef id = CFStringCreateWithCString(NULL, bundleID, kCFStringEncodingUTF8);
	CFArrayRef apps = id ? LSCopyApplicationURLsForBundleIdentifier(id, NULL) : NULL;
	CFURLRef app = NULL;

	if (apps != NULL && CFArrayGetCount(apps) > 0)
		app = CFRetain(CFArrayGetValueAtIndex(apps, 0));
	if (apps != NULL)
		CFRelease(apps);
	if (id != NULL)
		CFRelease(id);
	return app;
}

int
main(int argc, char *argv[])
{
	static const struct option longopts[] = {
		{ "reveal", no_argument, NULL, 'R' },
		{ "help", no_argument, NULL, 'h' },
		{ NULL, 0, NULL, 0 },
	};
	const char *appName = NULL, *bundleID = NULL;
	int reveal = 0, newInstance = 0, ch;
	CFURLRef app = NULL;

	while ((ch = getopt_long(argc, argv, "a:b:etRnh", longopts, NULL)) != -1) {
		switch (ch) {
		case 'a':
			appName = optarg;
			break;
		case 'b':
			bundleID = optarg;
			break;
		case 'e':
		case 't':
			bundleID = TEXT_EDITOR_ID;
			break;
		case 'R':
			reveal = 1;
			break;
		case 'n':
			newInstance = 1;
			break;
		default:
			usage();
		}
	}
	argc -= optind;
	argv += optind;
	if (argc == 0 && appName == NULL && bundleID == NULL)
		usage();
	if (reveal && argc == 0)
		usage();

	if (appName != NULL && (app = applicationNamed(appName)) == NULL)
		errx(1, "Unable to find application named '%s'", appName);
	if (bundleID != NULL && (app = applicationWithIdentifier(bundleID)) == NULL)
		errx(1, "Unable to find application with identifier '%s'", bundleID);

	/* The items, as absolute file URLs; each must exist. */
	CFMutableArrayRef items = CFArrayCreateMutable(NULL, argc, &kCFTypeArrayCallBacks);
	for (int i = 0; i < argc; i++) {
		char path[PATH_MAX];
		struct stat st;
		if (realpath(argv[i], path) == NULL || stat(path, &st) != 0)
			errx(1, "The file %s does not exist.", argv[i]);
		CFURLRef url = fileURL(path, S_ISDIR(st.st_mode));
		CFArrayAppendValue(items, url);
		CFRelease(url);
	}

	OSStatus status;
	if (reveal)
		status = LSRevealInFiles(items);
	else {
		LSLaunchURLSpec spec = { app, CFArrayGetCount(items) ? items : NULL, NULL,
		    kLSLaunchDefaults | (newInstance ? kLSLaunchNewInstance : 0), NULL };
		status = LSOpenFromURLSpec(&spec, NULL);
	}
	if (status == kLSApplicationNotFoundErr && argc > 0)
		errx(1, "No application knows how to open %s.", argv[0]);
	if (status != noErr)
		errx(1, "LaunchServices could not open the items (error %d).", (int)status);
	CFRelease(items);
	if (app != NULL)
		CFRelease(app);
	return 0;
}
