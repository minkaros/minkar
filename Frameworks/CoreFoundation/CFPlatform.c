/*
 * Copyright (c) 2015 Apple Inc. All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_LICENSE_HEADER_END@
 */

/*	CFPlatform.c
	Copyright (c) 1999-2014, Apple Inc. All rights reserved.
	Responsibility: Tony Parker
*/

#include "CFInternal.h"
#include <CoreFoundation/CFPriv.h>
    #include <stdlib.h>
    #include <sys/stat.h>
    #include <string.h>
    #include <unistd.h>
    #include <fcntl.h>
    #include <pwd.h>
    #include <crt_externs.h>
    #include <mach-o/dyld.h>
    #include <pthread/tsd_private.h>

#define kCFPlatformInterfaceStringEncoding	kCFStringEncodingUTF8

extern void __CFGetUGIDs(uid_t *euid, gid_t *egid);

// CoreGraphics and LaunchServices are only projects (1 Dec 2006) that use these
char **_CFArgv(void) { return *_NSGetArgv(); }
int _CFArgc(void) { return *_NSGetArgc(); }


CF_PRIVATE Boolean _CFGetCurrentDirectory(char *path, int maxlen) {
    return getcwd(path, maxlen) != NULL;
}

static const char *__CFProcessPath = NULL;
static const char *__CFprogname = NULL;

const char **_CFGetProgname(void) {
    if (!__CFprogname)
        _CFProcessPath();		// sets up __CFprogname as a side-effect
    return &__CFprogname;
}

const char **_CFGetProcessPath(void) {
    if (!__CFProcessPath)
        _CFProcessPath();		// sets up __CFProcessPath as a side-effect
    return &__CFProcessPath;
}

const char *_CFProcessPath(void) {
    if (__CFProcessPath) return __CFProcessPath;
    if (!issetugid()) {
	const char *path = (char *)__CFgetenv("CFProcessPath");
	if (path) {
	    __CFProcessPath = strdup(path);
	    __CFprogname = strrchr(__CFProcessPath, PATH_SEP);
	    __CFprogname = (__CFprogname ? __CFprogname + 1 : __CFProcessPath);
	    return __CFProcessPath;
	}
    }
    uint32_t size = CFMaxPathSize;
    char buffer[size];
    if (0 == _NSGetExecutablePath(buffer, &size)) {
	__CFProcessPath = strdup(buffer);
	__CFprogname = strrchr(__CFProcessPath, PATH_SEP);
	__CFprogname = (__CFprogname ? __CFprogname + 1 : __CFProcessPath);
    }
    if (!__CFProcessPath) {
	__CFProcessPath = "";
        __CFprogname = __CFProcessPath;
    }
    return __CFProcessPath;
}

CF_PRIVATE CFStringRef _CFProcessNameString(void) {
    static CFStringRef __CFProcessNameString = NULL;
    if (!__CFProcessNameString) {
        const char *processName = *_CFGetProgname();
        if (!processName) processName = "";
        CFStringRef newStr = CFStringCreateWithCString(kCFAllocatorSystemDefault, processName, kCFPlatformInterfaceStringEncoding);
        if (!OSAtomicCompareAndSwapPtrBarrier(NULL, (void *) newStr, (void * volatile *)& __CFProcessNameString)) {
            CFRelease(newStr);    // someone else made the assignment, so just release the extra string.
        }
    }
    return __CFProcessNameString;
}


#include <pwd.h>
#include <sys/param.h>

// Set the fallBackToHome parameter to true if we should fall back to the HOME environment variable if all else fails. Otherwise return NULL.
static CFURLRef _CFCopyHomeDirURLForUser(struct passwd *upwd, bool fallBackToHome) {
    const char *fixedHomePath = issetugid() ? NULL : __CFgetenv("CFFIXED_USER_HOME");
    const char *homePath = NULL;
    
    // Calculate the home directory we will use
    // First try CFFIXED_USER_HOME (only if not setugid), then fall back to the upwd, then fall back to HOME environment variable
    CFURLRef home = NULL;
    if (!issetugid() && fixedHomePath) home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)fixedHomePath, strlen(fixedHomePath), true);
    if (!home && upwd && upwd->pw_dir) home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)upwd->pw_dir, strlen(upwd->pw_dir), true);
    if (fallBackToHome && !home) homePath = __CFgetenv("HOME");
    if (fallBackToHome && !home && homePath) home = CFURLCreateFromFileSystemRepresentation(kCFAllocatorSystemDefault, (uint8_t *)homePath, strlen(homePath), true);
    
    return home;
}


#define CFMaxHostNameLength	256
#define CFMaxHostNameSize	(CFMaxHostNameLength+1)

CF_PRIVATE CFStringRef _CFStringCreateHostName(void) {
    char myName[CFMaxHostNameSize];

    // return @"" instead of nil a la CFUserName() and Ali Ozer
    if (0 != gethostname(myName, CFMaxHostNameSize)) myName[0] = '\0';
    return CFStringCreateWithCString(kCFAllocatorSystemDefault, myName, kCFPlatformInterfaceStringEncoding);
}

/* These are sanitized versions of the above functions. We might want to eliminate the above ones someday.
   These can return NULL.
*/
CF_EXPORT CFStringRef CFGetUserName(void) {
    return CFCopyUserName();
}

CF_EXPORT CFStringRef CFCopyUserName(void) {
    CFStringRef result = NULL;
    uid_t euid;
    __CFGetUGIDs(&euid, NULL);
    struct passwd *upwd = getpwuid(euid ? euid : getuid());
    if (upwd && upwd->pw_name) {
        result = CFStringCreateWithCString(kCFAllocatorSystemDefault, upwd->pw_name, kCFPlatformInterfaceStringEncoding);
    } else {
        const char *cuser = __CFgetenv("USER");
        if (cuser) {
            result = CFStringCreateWithCString(kCFAllocatorSystemDefault, cuser, kCFPlatformInterfaceStringEncoding);
        }
    }
    if (!result)
        result = (CFStringRef)CFRetain(CFSTR(""));
    return result;
}

CFURLRef CFCopyHomeDirectoryURL(void) {
    uid_t euid;
    __CFGetUGIDs(&euid, NULL);
    struct passwd *upwd = getpwuid(euid ? euid : getuid());
    return _CFCopyHomeDirURLForUser(upwd, true);
}

CF_EXPORT CFURLRef CFCopyHomeDirectoryURLForUser(CFStringRef uName) {
    if (!uName) {
        uid_t euid;
        __CFGetUGIDs(&euid, NULL);
        struct passwd *upwd = getpwuid(euid ? euid : getuid());
        return _CFCopyHomeDirURLForUser(upwd, true);
    } else {
        struct passwd *upwd = NULL;
        char buf[128], *user;
        SInt32 len = CFStringGetLength(uName), size = CFStringGetMaximumSizeForEncoding(len, kCFPlatformInterfaceStringEncoding);
        CFIndex usedSize;
        if (size < 127) {
            user = buf;
        } else {
            user = CFAllocatorAllocate(kCFAllocatorSystemDefault, size+1, 0);
        }
        if (CFStringGetBytes(uName, CFRangeMake(0, len), kCFPlatformInterfaceStringEncoding, 0, true, (uint8_t *)user, size, &usedSize) == len) {
            user[usedSize] = '\0';
            upwd = getpwnam(user);
        }
        if (buf != user) {
            CFAllocatorDeallocate(kCFAllocatorSystemDefault, user);
        }
        return _CFCopyHomeDirURLForUser(upwd, false);
    }
}


#undef CFMaxHostNameLength
#undef CFMaxHostNameSize

#pragma mark -
#pragma mark Thread Functions

#pragma mark -
#pragma mark Thread Local Data

// If slot >= CF_TSD_MAX_SLOTS, the SPI functions will crash at NULL + slot address.
// If thread data has been torn down, these functions should crash on CF_TSD_BAD_PTR + slot address.
#define CF_TSD_MAX_SLOTS 70

static const unsigned long CF_TSD_KEY = __PTK_FRAMEWORK_COREFOUNDATION_KEY5;

// Windows and Linux, not sure how many times the destructor could get called; CF_TSD_MAX_DESTRUCTOR_CALLS could be 1

#define CF_TSD_BAD_PTR ((void *)0x1000)

typedef void (*tsdDestructor)(void *);

// Data structure to hold TSD data, cleanup functions for each
typedef struct __CFTSDTable {
    uint32_t destructorCount;
    uintptr_t data[CF_TSD_MAX_SLOTS];
    tsdDestructor destructors[CF_TSD_MAX_SLOTS];
} __CFTSDTable;

static void __CFTSDFinalize(void *arg);

static void __CFTSDSetSpecific(void *arg) {
    _pthread_setspecific_direct(CF_TSD_KEY, arg);
}

static void *__CFTSDGetSpecific() {
    return _pthread_getspecific_direct(CF_TSD_KEY);
}

static void __CFTSDFinalize(void *arg) {
    // Set our TSD so we're called again by pthreads. It will call the destructor PTHREAD_DESTRUCTOR_ITERATIONS times as long as a value is set in the thread specific data. We handle each case below.
    __CFTSDSetSpecific(arg);

    if (!arg || arg == CF_TSD_BAD_PTR) {
        // We've already been destroyed. The call above set the bad pointer again. Now we just return.
        return;
    }
    
    __CFTSDTable *table = (__CFTSDTable *)arg;
    table->destructorCount++;
        
    // On first calls invoke destructor. Later we destroy the data.
    // Note that invocation of the destructor may cause a value to be set again in the per-thread data slots. The destructor count and destructors are preserved.  
    // This logic is basically the same as what pthreads does. We just skip the 'created' flag.
    for (int32_t i = 0; i < CF_TSD_MAX_SLOTS; i++) {
        if (table->data[i] && table->destructors[i]) {
            uintptr_t old = table->data[i];
            table->data[i] = (uintptr_t)NULL;
            table->destructors[i]((void *)(old));
        }
    }
    
    if (table->destructorCount == PTHREAD_DESTRUCTOR_ITERATIONS - 1) {    // On PTHREAD_DESTRUCTOR_ITERATIONS-1 call, destroy our data
        free(table);
        
        // Now if the destructor is called again we will take the shortcut at the beginning of this function.
        __CFTSDSetSpecific(CF_TSD_BAD_PTR);
        return;
    }
}

extern int pthread_key_init_np(int, void (*)(void *));

// Get or initialize a thread local storage. It is created on demand.
static __CFTSDTable *__CFTSDGetTable() {
    __CFTSDTable *table = (__CFTSDTable *)__CFTSDGetSpecific();
    // Make sure we're not setting data again after destruction.
    if (table == CF_TSD_BAD_PTR) {
        return NULL;
    }
    // Create table on demand
    if (!table) {
        // This memory is freed in the finalize function
        table = (__CFTSDTable *)calloc(1, sizeof(__CFTSDTable));
        // Windows and Linux have created the table already, we need to initialize it here for other platforms. On Windows, the cleanup function is called by DllMain when a thread exits. On Linux the destructor is set at init time.
        pthread_key_init_np(CF_TSD_KEY, __CFTSDFinalize);
        __CFTSDSetSpecific(table);
    }
    
    return table;
}


// For the use of CF and Foundation only
CF_EXPORT void *_CFGetTSD(uint32_t slot) {
    if (slot > CF_TSD_MAX_SLOTS) {
        _CFLogSimple(kCFLogLevelError, "Error: TSD slot %d out of range (get)", slot);
        HALT;
    }
    __CFTSDTable *table = __CFTSDGetTable();
    if (!table) {
        // Someone is getting TSD during thread destruction. The table is gone, so we can't get any data anymore.
        _CFLogSimple(kCFLogLevelWarning, "Warning: TSD slot %d retrieved but the thread data has already been torn down.", slot);
        return NULL;
    }
    uintptr_t *slots = (uintptr_t *)(table->data);
    return (void *)slots[slot];
}

// For the use of CF and Foundation only
CF_EXPORT void *_CFSetTSD(uint32_t slot, void *newVal, tsdDestructor destructor) {
    if (slot > CF_TSD_MAX_SLOTS) {
        _CFLogSimple(kCFLogLevelError, "Error: TSD slot %d out of range (set)", slot);
        HALT;
    }
    __CFTSDTable *table = __CFTSDGetTable();
    if (!table) {
        // Someone is setting TSD during thread destruction. The table is gone, so we can't get any data anymore.
        _CFLogSimple(kCFLogLevelWarning, "Warning: TSD slot %d set but the thread data has already been torn down.", slot);
        return NULL;
    }

    void *oldVal = (void *)table->data[slot];
    
    table->data[slot] = (uintptr_t)newVal;
    table->destructors[slot] = destructor;
    
    return oldVal;
}


#pragma mark -
#pragma mark Windows Wide to UTF8 and UTF8 to Wide

#pragma mark -
#pragma mark Linux OSAtomic

#pragma mark -
#pragma mark Windows and Linux Helpers

