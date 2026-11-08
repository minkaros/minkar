/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * LaunchServices' opening and information (LSOpen.h, LSInfo.h), on
 * launchservicesd (LSSession.h, interface version 3). The caller looks at
 * the file (its extension, whether it is a folder, text or an
 * executable): the daemon reads no one's files. An application is
 * launched by the caller (posix_spawn, the child collected when it
 * exits); its documents wait in the daemon until it checks in.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <dispatch/dispatch.h>
#include <fcntl.h>
#include <spawn.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "LSInfo.h"
#include "LSOpen.h"
#include "LSSession.h"

extern char **environ;

#define FILES_ID	"org.minkar.Files"

/* -- What a file is -- */

static Boolean
copyPath(CFURLRef url, char path[1024])
{
	return url != NULL && CFURLGetFileSystemRepresentation(url, true, (UInt8 *)path, 1024);
}

/* Text: no NUL, and UTF-8 that holds together (an empty file too). */
static int
looksLikeText(const char *path)
{
	unsigned char buf[4096];
	int fd = open(path, O_RDONLY);
	ssize_t n;

	if (fd < 0)
		return 0;
	n = read(fd, buf, sizeof(buf));
	close(fd);
	if (n < 0)
		return 0;
	for (ssize_t i = 0; i < n; i++) {
		unsigned c = buf[i], follow;
		if (c == 0)
			return 0;
		if (c < 0x80)
			continue;
		follow = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 9;
		if (follow == 9)
			return 0;
		for (unsigned k = 1; k <= follow; k++)
			if (i + k < n && (buf[i + k] & 0xC0) != 0x80)
				return 0;
		i += follow;	/* a sequence cut at the end of the buffer is fine */
	}
	return 1;
}

static int
describe(const char *path, LSSessionFile *file, LSRolesMask roles)
{
	struct stat st;
	const char *slash = strrchr(path, '/'), *name = slash ? slash + 1 : path;
	const char *dot = strrchr(name, '.');

	memset(file, 0, sizeof(*file));
	file->roles = roles;
	if (stat(path, &st) != 0)
		return -1;
	if (dot != NULL && dot != name && strlen(dot + 1) < sizeof(file->extension))
		for (size_t i = 0; dot[1 + i]; i++)
			file->extension[i] = (char)(dot[1 + i] >= 'A' && dot[1 + i] <= 'Z' ?
			    dot[1 + i] + ('a' - 'A') : dot[1 + i]);
	if (S_ISDIR(st.st_mode))
		file->flags |= kLSSessionFileIsDirectory;
	else if (S_ISREG(st.st_mode)) {
		if (looksLikeText(path))
			file->flags |= kLSSessionFileIsText;
		if (st.st_mode & 0111)
			file->flags |= kLSSessionFileIsExecutable;
	}
	return 0;
}

static OSStatus
statusFor(int session)
{
	switch (session) {
	case kLSSessionSuccess:		return noErr;
	case kLSSessionNoSuchApp:	return kLSApplicationNotFoundErr;
	case kLSSessionIllegalArgument:	return kLSDataErr;
	case kLSSessionNotAllowed:	return kLSNoLaunchPermissionErr;
	default:			return kLSServerCommunicationErr;
	}
}

static CFURLRef
urlForPath(const char *path, Boolean isDirectory)
{
	return CFURLCreateFromFileSystemRepresentation(NULL, (const UInt8 *)path,
	    (CFIndex)strlen(path), isDirectory);
}

/* The applications for a file, best first; NULL on an error. */
static LSSessionPath *
copyHandlers(CFURLRef url, LSRolesMask roles, uint32_t *count, OSStatus *status)
{
	char path[1024];
	LSSessionFile file;
	LSSessionPath *apps = NULL;

	*count = 0;
	if (!copyPath(url, path) || describe(path, &file, roles) != 0) {
		*status = kLSDataErr;
		return NULL;
	}
	*status = statusFor(LSSessionCopyHandlers(&file, &apps, count, NULL));
	if (*status != noErr) {
		free(apps);
		return NULL;
	}
	if (*count == 0)
		*status = kLSApplicationNotFoundErr;
	return apps;
}

static void
setError(CFErrorRef *outError, OSStatus status)
{
	if (outError != NULL)
		*outError = CFErrorCreate(NULL, kCFErrorDomainOSStatus, status, NULL);
}

/* -- LSInfo.h -- */

CFURLRef
LSCopyDefaultApplicationURLForURL(CFURLRef inURL, LSRolesMask inRoleMask, CFErrorRef *outError)
{
	uint32_t count;
	OSStatus status;
	LSSessionPath *apps = copyHandlers(inURL, inRoleMask, &count, &status);
	CFURLRef app = NULL;

	if (apps != NULL && count > 0)
		app = urlForPath(apps[0].path, true);
	else
		setError(outError, status);
	free(apps);
	return app;
}

CFArrayRef
LSCopyApplicationURLsForURL(CFURLRef inURL, LSRolesMask inRoleMask)
{
	uint32_t count;
	OSStatus status;
	LSSessionPath *apps = copyHandlers(inURL, inRoleMask, &count, &status);

	if (apps == NULL || count == 0) {
		free(apps);
		return NULL;
	}
	CFMutableArrayRef list = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);
	for (uint32_t i = 0; i < count; i++) {
		CFURLRef app = urlForPath(apps[i].path, true);
		if (app != NULL) {
			CFArrayAppendValue(list, app);
			CFRelease(app);
		}
	}
	free(apps);
	return list;
}

CFArrayRef
LSCopyApplicationURLsForBundleIdentifier(CFStringRef inBundleIdentifier, CFErrorRef *outError)
{
	char id[1024];
	LSSessionPath app;

	if (inBundleIdentifier == NULL ||
	    !CFStringGetCString(inBundleIdentifier, id, sizeof(id), kCFStringEncodingUTF8)) {
		setError(outError, kLSDataErr);
		return NULL;
	}
	OSStatus status = statusFor(LSSessionCopyAppForBundleIdentifier(id, &app));
	if (status != noErr) {
		setError(outError, status);
		return NULL;
	}
	CFURLRef url = urlForPath(app.path, true);
	CFArrayRef list = CFArrayCreate(NULL, (const void **)&url, 1, &kCFTypeArrayCallBacks);
	CFRelease(url);
	return list;
}

/* -- By type identifier and by URL scheme -- */

static Boolean
cString(CFStringRef string, char buffer[1024])
{
	return string != NULL && CFStringGetCString(string, buffer, 1024, kCFStringEncodingUTF8);
}

/* The bundle identifiers of applications, in order (those without one
 * left out). */
static CFArrayRef
createBundleIdentifiers(const LSSessionPath *apps, uint32_t count)
{
	CFMutableArrayRef list = CFArrayCreateMutable(NULL, count, &kCFTypeArrayCallBacks);

	for (uint32_t i = 0; i < count; i++) {
		CFURLRef url = urlForPath(apps[i].path, true);
		CFBundleRef bundle = url ? CFBundleCreate(NULL, url) : NULL;
		CFStringRef id = bundle ? CFBundleGetIdentifier(bundle) : NULL;
		if (id != NULL && !CFArrayContainsValue(list, CFRangeMake(0, CFArrayGetCount(list)), id))
			CFArrayAppendValue(list, id);
		if (bundle != NULL)
			CFRelease(bundle);
		if (url != NULL)
			CFRelease(url);
	}
	if (CFArrayGetCount(list) == 0) {
		CFRelease(list);
		return NULL;
	}
	return list;
}

static LSSessionPath *
copyTypeHandlers(CFStringRef type, LSRolesMask roles, uint32_t *count, OSStatus *status)
{
	char uti[1024];
	LSSessionPath *apps = NULL;

	*count = 0;
	if (!cString(type, uti)) {
		*status = kLSDataErr;
		return NULL;
	}
	*status = statusFor(LSSessionCopyHandlersForType(uti, roles == kLSRolesAll ? 0 : roles,
	    &apps, count));
	if (*status == noErr && *count == 0)
		*status = kLSApplicationNotFoundErr;
	if (*status != noErr) {
		free(apps);
		return NULL;
	}
	return apps;
}

static LSSessionPath *
copySchemeHandlers(CFStringRef scheme, uint32_t *count, OSStatus *status)
{
	char name[1024];
	LSSessionPath *apps = NULL;

	*count = 0;
	if (!cString(scheme, name)) {
		*status = kLSDataErr;
		return NULL;
	}
	*status = statusFor(LSSessionCopyHandlersForScheme(name, &apps, count));
	if (*status == noErr && *count == 0)
		*status = kLSApplicationNotFoundErr;
	if (*status != noErr) {
		free(apps);
		return NULL;
	}
	return apps;
}

CFArrayRef
LSCopyAllRoleHandlersForContentType(CFStringRef inContentType, LSRolesMask inRole)
{
	uint32_t count;
	OSStatus status;
	LSSessionPath *apps = copyTypeHandlers(inContentType, inRole, &count, &status);
	CFArrayRef list = apps ? createBundleIdentifiers(apps, count) : NULL;

	free(apps);
	return list;
}

CFStringRef
LSCopyDefaultRoleHandlerForContentType(CFStringRef inContentType, LSRolesMask inRole)
{
	CFArrayRef list = LSCopyAllRoleHandlersForContentType(inContentType, inRole);
	CFStringRef id = list ? CFRetain(CFArrayGetValueAtIndex(list, 0)) : NULL;

	if (list != NULL)
		CFRelease(list);
	return id;
}

CFURLRef
LSCopyDefaultApplicationURLForContentType(CFStringRef inContentType, LSRolesMask inRoleMask,
    CFErrorRef *outError)
{
	uint32_t count;
	OSStatus status;
	LSSessionPath *apps = copyTypeHandlers(inContentType, inRoleMask, &count, &status);
	CFURLRef app = apps ? urlForPath(apps[0].path, true) : NULL;

	if (app == NULL)
		setError(outError, status);
	free(apps);
	return app;
}

CFArrayRef
LSCopyAllHandlersForURLScheme(CFStringRef inURLScheme)
{
	uint32_t count;
	OSStatus status;
	LSSessionPath *apps = copySchemeHandlers(inURLScheme, &count, &status);
	CFArrayRef list = apps ? createBundleIdentifiers(apps, count) : NULL;

	free(apps);
	return list;
}

CFStringRef
LSCopyDefaultHandlerForURLScheme(CFStringRef inURLScheme)
{
	CFArrayRef list = LSCopyAllHandlersForURLScheme(inURLScheme);
	CFStringRef id = list ? CFRetain(CFArrayGetValueAtIndex(list, 0)) : NULL;

	if (list != NULL)
		CFRelease(list);
	return id;
}

OSStatus
LSRegisterURL(CFURLRef inURL, Boolean inUpdate)
{
	char path[1024];

	(void)inUpdate;
	if (!copyPath(inURL, path))
		return kLSDataErr;
	return statusFor(LSSessionRegister(path));
}

/* -- Launching -- */

/* The executable of an application bundle (or the file itself). */
static int
executableFor(const char *app, char exe[1024])
{
	struct stat st;

	if (stat(app, &st) != 0)
		return -1;
	if (!S_ISDIR(st.st_mode)) {
		strlcpy(exe, app, 1024);
		return (st.st_mode & 0111) ? 0 : -1;
	}
	CFURLRef url = urlForPath(app, true);
	CFBundleRef bundle = url ? CFBundleCreate(NULL, url) : NULL;
	CFURLRef exeURL = bundle ? CFBundleCopyExecutableURL(bundle) : NULL;
	int ok = exeURL != NULL && copyPath(exeURL, exe) && access(exe, X_OK) == 0;
	if (exeURL != NULL)
		CFRelease(exeURL);
	if (bundle != NULL)
		CFRelease(bundle);
	if (url != NULL)
		CFRelease(url);
	return ok ? 0 : -1;
}

/* A running instance of the application, or 0. */
static pid_t
runningPID(const char *app)
{
	LSSessionApp *apps = NULL;
	uint32_t count = 0;
	char want[1024], have[1024];
	pid_t pid = 0;

	if (realpath(app, want) == NULL)
		strlcpy(want, app, sizeof(want));
	if (LSSessionCopyApps(&apps, &count) != kLSSessionSuccess)
		return 0;
	for (uint32_t i = 0; i < count && pid == 0; i++) {
		if (realpath(apps[i].path, have) == NULL)
			strlcpy(have, apps[i].path, sizeof(have));
		if (strcmp(have, want) == 0 && apps[i].pid != getpid())
			pid = apps[i].pid;
	}
	free(apps);
	return pid;
}

/* Runs the executable, with these arguments after its path (strings). */
static OSStatus
spawn(const char *exe, CFArrayRef arguments)
{
	CFIndex count = arguments ? CFArrayGetCount(arguments) : 0;
	char **argv = calloc((size_t)count + 2, sizeof(char *));
	posix_spawnattr_t attr;
	pid_t pid;
	int error;

	if (argv == NULL)
		return kLSNoExecutableErr;
	argv[0] = (char *)exe;
	for (CFIndex i = 0; i < count; i++) {
		CFStringRef string = CFArrayGetValueAtIndex(arguments, i);
		CFIndex size = CFStringGetMaximumSizeForEncoding(CFStringGetLength(string),
		    kCFStringEncodingUTF8) + 1;

		argv[i + 1] = malloc((size_t)size);
		if (argv[i + 1] == NULL || !CFStringGetCString(string, argv[i + 1], size,
		    kCFStringEncodingUTF8)) {
			free(argv[i + 1]);
			argv[i + 1] = strdup("");
		}
	}

	posix_spawnattr_init(&attr);
	/* Its own process group: a signal to the caller's is not its own. */
	posix_spawnattr_setflags(&attr, POSIX_SPAWN_SETPGROUP);
	posix_spawnattr_setpgroup(&attr, 0);
	error = posix_spawn(&pid, exe, NULL, &attr, argv, environ);
	posix_spawnattr_destroy(&attr);
	for (CFIndex i = 0; i < count; i++)
		free(argv[i + 1]);
	free(argv);
	if (error != 0)
		return kLSNoExecutableErr;
	/* The child is collected when it exits (no zombie in the caller). */
	dispatch_source_t exit = dispatch_source_create(DISPATCH_SOURCE_TYPE_PROC, (uintptr_t)pid,
	    DISPATCH_PROC_EXIT, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0));
	if (exit != NULL) {
		dispatch_source_set_event_handler(exit, ^{
			waitpid(pid, NULL, WNOHANG);
			dispatch_source_cancel(exit);
			dispatch_release(exit);
		});
		dispatch_resume(exit);
	}
	return noErr;
}

/* URLs as the daemon takes them: absolute strings, each with its NUL. */
static char *
packURLs(CFArrayRef urls, uint32_t *length)
{
	char *bytes = malloc(kLSSessionMaxURLBytes);
	uint32_t n = 0;

	*length = 0;
	if (bytes == NULL)
		return NULL;
	for (CFIndex i = 0; urls != NULL && i < CFArrayGetCount(urls); i++) {
		CFURLRef url = CFArrayGetValueAtIndex(urls, i);
		CFURLRef absolute = CFURLCopyAbsoluteURL(url);
		CFStringRef string = absolute ? CFURLGetString(absolute) : NULL;
		char buffer[4096];
		if (string != NULL && CFStringGetCString(string, buffer, sizeof(buffer), kCFStringEncodingUTF8)) {
			size_t len = strlen(buffer) + 1;
			if (n + len <= kLSSessionMaxURLBytes) {
				memcpy(bytes + n, buffer, len);
				n += (uint32_t)len;
			}
		}
		if (absolute != NULL)
			CFRelease(absolute);
	}
	*length = n;
	return bytes;
}

/* The application opens these items (or just runs, none given). */
static OSStatus
openWith(const char *app, CFArrayRef items, uint32_t flags, LSLaunchFlags launchFlags,
    CFArrayRef arguments, CFURLRef *outLaunchedURL)
{
	char exe[1024];
	uint32_t length = 0;
	int running = 0;
	OSStatus status = noErr;

	if (executableFor(app, exe) != 0)
		return kLSNoExecutableErr;
	if (outLaunchedURL != NULL)
		*outLaunchedURL = urlForPath(app, true);
	char *urls = packURLs(items, &length);
	if (length > 0)
		status = statusFor(LSSessionOpen(app, urls, length, flags, &running));
	free(urls);
	if (status != noErr)
		return status;
	if (running && !(launchFlags & kLSLaunchNewInstance))
		return noErr;
	pid_t pid = (launchFlags & kLSLaunchNewInstance) ? 0 : runningPID(app);
	if (pid != 0) {
		if (!(launchFlags & kLSLaunchDontSwitch))
			LSSessionActivate(pid, 2 /* NSApplicationActivateIgnoringOtherApps */);
		return noErr;
	}
	return spawn(exe, arguments);
}

static int
isApplication(const char *path)
{
	size_t n = strlen(path);
	struct stat st;

	while (n > 1 && path[n - 1] == '/')
		n--;
	return n > 4 && strncasecmp(path + n - 4, ".app", 4) == 0 && stat(path, &st) == 0 &&
	    S_ISDIR(st.st_mode);
}

OSStatus
LSOpenFromURLSpec(const LSLaunchURLSpec *inLaunchSpec, CFURLRef *outLaunchedURL)
{
	return LSOpenFromURLSpecWithArguments(inLaunchSpec, NULL, outLaunchedURL);
}

OSStatus
LSOpenFromURLSpecWithArguments(const LSLaunchURLSpec *inLaunchSpec, CFArrayRef inArguments,
    CFURLRef *outLaunchedURL)
{
	char path[1024];

	if (outLaunchedURL != NULL)
		*outLaunchedURL = NULL;
	if (inLaunchSpec == NULL)
		return kLSDataErr;
	if (inLaunchSpec->appURL != NULL) {
		if (!copyPath(inLaunchSpec->appURL, path))
			return kLSDataErr;
		return openWith(path, inLaunchSpec->itemURLs, 0, inLaunchSpec->launchFlags, inArguments,
		    outLaunchedURL);
	}
	CFArrayRef items = inLaunchSpec->itemURLs;
	CFIndex count = items ? CFArrayGetCount(items) : 0;
	OSStatus status = count ? noErr : kLSDataErr;
	for (CFIndex i = 0; i < count; i++) {
		CFURLRef item = CFArrayGetValueAtIndex(items, i);
		OSStatus one;
		if (!copyPath(item, path)) {
			/* Not a file: the application of its scheme. */
			CFStringRef scheme = CFURLCopyScheme(item);
			uint32_t n;
			LSSessionPath *apps = scheme ? copySchemeHandlers(scheme, &n, &one) : NULL;
			if (scheme == NULL)
				one = kLSDataErr;
			if (apps != NULL) {
				CFArrayRef one_item = CFArrayCreate(NULL, (const void **)&item, 1,
				    &kCFTypeArrayCallBacks);
				one = openWith(apps[0].path, one_item, 0, inLaunchSpec->launchFlags,
				    inArguments, i == 0 ? outLaunchedURL : NULL);
				CFRelease(one_item);
			}
			free(apps);
			if (scheme != NULL)
				CFRelease(scheme);
		} else if (isApplication(path))
			one = openWith(path, NULL, 0, inLaunchSpec->launchFlags, inArguments,
			    i == 0 ? outLaunchedURL : NULL);
		else {
			uint32_t n;
			LSSessionPath *apps = copyHandlers(item, kLSRolesAll, &n, &one);
			if (apps != NULL && n > 0) {
				CFArrayRef one_item = CFArrayCreate(NULL, (const void **)&item, 1,
				    &kCFTypeArrayCallBacks);
				one = openWith(apps[0].path, one_item, 0, inLaunchSpec->launchFlags,
				    inArguments, i == 0 ? outLaunchedURL : NULL);
				CFRelease(one_item);
			}
			free(apps);
		}
		if (one != noErr)
			status = one;
	}
	return status;
}

OSStatus
LSOpenCFURLRef(CFURLRef inURL, CFURLRef *outLaunchedURL)
{
	if (inURL == NULL)
		return kLSDataErr;
	CFArrayRef items = CFArrayCreate(NULL, (const void **)&inURL, 1, &kCFTypeArrayCallBacks);
	LSLaunchURLSpec spec = { NULL, items, NULL, kLSLaunchDefaults, NULL };
	OSStatus status = LSOpenFromURLSpec(&spec, outLaunchedURL);
	CFRelease(items);
	return status;
}

OSStatus
LSRevealInFiles(CFArrayRef inItemURLs)
{
	LSSessionPath files;

	if (inItemURLs == NULL || CFArrayGetCount(inItemURLs) == 0)
		return kLSDataErr;
	OSStatus status = statusFor(LSSessionCopyAppForBundleIdentifier(FILES_ID, &files));
	if (status != noErr)
		return status;
	return openWith(files.path, inItemURLs, kLSSessionOpenReveal, kLSLaunchDefaults, NULL, NULL);
}
