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
 * NSURL's resource values (file properties by key), bookmarks, and the
 * URL methods of NSFileManager and NSBundle.
 *
 * Values are read from the file system each time (none is cached). The
 * type identifiers are those of a small table of extensions: there is no
 * LaunchServices database below Foundation. A bookmark is Minkar's own
 * property list (the path, the file's identity and the values asked for);
 * a program only keeps it and hands it back.
 */

#import <Foundation/NSURL.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSBundle.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSData.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSDirectoryEnumerator.h>
#import <Foundation/NSError.h>
#import <Foundation/NSException.h>
#import <Foundation/NSFileManager.h>
#import <Foundation/NSNumber.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSProcessInfo.h>
#import <Foundation/NSPropertyList.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSString.h>
#import <Foundation/FoundationErrors.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <pwd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/attr.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/xattr.h>
#include <unistd.h>

NSURLResourceKey const NSURLNameKey = @"NSURLNameKey";
NSURLResourceKey const NSURLLocalizedNameKey = @"NSURLLocalizedNameKey";
NSURLResourceKey const NSURLIsRegularFileKey = @"NSURLIsRegularFileKey";
NSURLResourceKey const NSURLIsDirectoryKey = @"NSURLIsDirectoryKey";
NSURLResourceKey const NSURLIsSymbolicLinkKey = @"NSURLIsSymbolicLinkKey";
NSURLResourceKey const NSURLIsVolumeKey = @"NSURLIsVolumeKey";
NSURLResourceKey const NSURLIsPackageKey = @"NSURLIsPackageKey";
NSURLResourceKey const NSURLIsApplicationKey = @"NSURLIsApplicationKey";
NSURLResourceKey const NSURLIsSystemImmutableKey = @"NSURLIsSystemImmutableKey";
NSURLResourceKey const NSURLIsUserImmutableKey = @"NSURLIsUserImmutableKey";
NSURLResourceKey const NSURLIsHiddenKey = @"NSURLIsHiddenKey";
NSURLResourceKey const NSURLHasHiddenExtensionKey = @"NSURLHasHiddenExtensionKey";
NSURLResourceKey const NSURLCreationDateKey = @"NSURLCreationDateKey";
NSURLResourceKey const NSURLContentAccessDateKey = @"NSURLContentAccessDateKey";
NSURLResourceKey const NSURLContentModificationDateKey = @"NSURLContentModificationDateKey";
NSURLResourceKey const NSURLAttributeModificationDateKey = @"NSURLAttributeModificationDateKey";
NSURLResourceKey const NSURLLinkCountKey = @"NSURLLinkCountKey";
NSURLResourceKey const NSURLParentDirectoryURLKey = @"NSURLParentDirectoryURLKey";
NSURLResourceKey const NSURLVolumeURLKey = @"NSURLVolumeURLKey";
NSURLResourceKey const NSURLTypeIdentifierKey = @"NSURLTypeIdentifierKey";
NSURLResourceKey const NSURLLocalizedTypeDescriptionKey = @"NSURLLocalizedTypeDescriptionKey";
NSURLResourceKey const NSURLLabelNumberKey = @"NSURLLabelNumberKey";
NSURLResourceKey const NSURLLabelColorKey = @"NSURLLabelColorKey";
NSURLResourceKey const NSURLLocalizedLabelKey = @"NSURLLocalizedLabelKey";
NSURLResourceKey const NSURLEffectiveIconKey = @"NSURLEffectiveIconKey";
NSURLResourceKey const NSURLCustomIconKey = @"NSURLCustomIconKey";
NSURLResourceKey const NSURLFileResourceIdentifierKey = @"NSURLFileResourceIdentifierKey";
NSURLResourceKey const NSURLVolumeIdentifierKey = @"NSURLVolumeIdentifierKey";
NSURLResourceKey const NSURLPreferredIOBlockSizeKey = @"NSURLPreferredIOBlockSizeKey";
NSURLResourceKey const NSURLIsReadableKey = @"NSURLIsReadableKey";
NSURLResourceKey const NSURLIsWritableKey = @"NSURLIsWritableKey";
NSURLResourceKey const NSURLIsExecutableKey = @"NSURLIsExecutableKey";
NSURLResourceKey const NSURLFileSecurityKey = @"NSURLFileSecurityKey";
NSURLResourceKey const NSURLIsExcludedFromBackupKey = @"NSURLIsExcludedFromBackupKey";
NSURLResourceKey const NSURLTagNamesKey = @"NSURLTagNamesKey";
NSURLResourceKey const NSURLPathKey = @"NSURLPathKey";
NSURLResourceKey const NSURLCanonicalPathKey = @"NSURLCanonicalPathKey";
NSURLResourceKey const NSURLIsMountTriggerKey = @"NSURLIsMountTriggerKey";
NSURLResourceKey const NSURLGenerationIdentifierKey = @"NSURLGenerationIdentifierKey";
NSURLResourceKey const NSURLDocumentIdentifierKey = @"NSURLDocumentIdentifierKey";
NSURLResourceKey const NSURLAddedToDirectoryDateKey = @"NSURLAddedToDirectoryDateKey";
NSURLResourceKey const NSURLQuarantinePropertiesKey = @"NSURLQuarantinePropertiesKey";
NSURLResourceKey const NSURLFileResourceTypeKey = @"NSURLFileResourceTypeKey";
NSURLResourceKey const NSURLThumbnailDictionaryKey = @"NSURLThumbnailDictionaryKey";
NSURLResourceKey const NSURLApplicationIsScriptableKey = @"NSURLApplicationIsScriptableKey";
NSURLResourceKey const NSURLIsAliasFileKey = @"NSURLIsAliasFileKey";
NSURLResourceKey const NSURLFileSizeKey = @"NSURLFileSizeKey";
NSURLResourceKey const NSURLFileAllocatedSizeKey = @"NSURLFileAllocatedSizeKey";
NSURLResourceKey const NSURLTotalFileSizeKey = @"NSURLTotalFileSizeKey";
NSURLResourceKey const NSURLTotalFileAllocatedSizeKey = @"NSURLTotalFileAllocatedSizeKey";
NSURLResourceKey const NSURLVolumeLocalizedFormatDescriptionKey = @"NSURLVolumeLocalizedFormatDescriptionKey";
NSURLResourceKey const NSURLVolumeTotalCapacityKey = @"NSURLVolumeTotalCapacityKey";
NSURLResourceKey const NSURLVolumeAvailableCapacityKey = @"NSURLVolumeAvailableCapacityKey";
NSURLResourceKey const NSURLVolumeResourceCountKey = @"NSURLVolumeResourceCountKey";
NSURLResourceKey const NSURLVolumeSupportsPersistentIDsKey = @"NSURLVolumeSupportsPersistentIDsKey";
NSURLResourceKey const NSURLVolumeSupportsSymbolicLinksKey = @"NSURLVolumeSupportsSymbolicLinksKey";
NSURLResourceKey const NSURLVolumeSupportsHardLinksKey = @"NSURLVolumeSupportsHardLinksKey";
NSURLResourceKey const NSURLVolumeSupportsJournalingKey = @"NSURLVolumeSupportsJournalingKey";
NSURLResourceKey const NSURLVolumeIsJournalingKey = @"NSURLVolumeIsJournalingKey";
NSURLResourceKey const NSURLVolumeSupportsSparseFilesKey = @"NSURLVolumeSupportsSparseFilesKey";
NSURLResourceKey const NSURLVolumeSupportsZeroRunsKey = @"NSURLVolumeSupportsZeroRunsKey";
NSURLResourceKey const NSURLVolumeSupportsCaseSensitiveNamesKey = @"NSURLVolumeSupportsCaseSensitiveNamesKey";
NSURLResourceKey const NSURLVolumeSupportsCasePreservedNamesKey = @"NSURLVolumeSupportsCasePreservedNamesKey";
NSURLResourceKey const NSURLVolumeSupportsRootDirectoryDatesKey = @"NSURLVolumeSupportsRootDirectoryDatesKey";
NSURLResourceKey const NSURLVolumeSupportsVolumeSizesKey = @"NSURLVolumeSupportsVolumeSizesKey";
NSURLResourceKey const NSURLVolumeSupportsRenamingKey = @"NSURLVolumeSupportsRenamingKey";
NSURLResourceKey const NSURLVolumeSupportsAdvisoryFileLockingKey = @"NSURLVolumeSupportsAdvisoryFileLockingKey";
NSURLResourceKey const NSURLVolumeSupportsExtendedSecurityKey = @"NSURLVolumeSupportsExtendedSecurityKey";
NSURLResourceKey const NSURLVolumeIsBrowsableKey = @"NSURLVolumeIsBrowsableKey";
NSURLResourceKey const NSURLVolumeMaximumFileSizeKey = @"NSURLVolumeMaximumFileSizeKey";
NSURLResourceKey const NSURLVolumeIsEjectableKey = @"NSURLVolumeIsEjectableKey";
NSURLResourceKey const NSURLVolumeIsRemovableKey = @"NSURLVolumeIsRemovableKey";
NSURLResourceKey const NSURLVolumeIsInternalKey = @"NSURLVolumeIsInternalKey";
NSURLResourceKey const NSURLVolumeIsAutomountedKey = @"NSURLVolumeIsAutomountedKey";
NSURLResourceKey const NSURLVolumeIsLocalKey = @"NSURLVolumeIsLocalKey";
NSURLResourceKey const NSURLVolumeIsReadOnlyKey = @"NSURLVolumeIsReadOnlyKey";
NSURLResourceKey const NSURLVolumeCreationDateKey = @"NSURLVolumeCreationDateKey";
NSURLResourceKey const NSURLVolumeURLForRemountingKey = @"NSURLVolumeURLForRemountingKey";
NSURLResourceKey const NSURLVolumeUUIDStringKey = @"NSURLVolumeUUIDStringKey";
NSURLResourceKey const NSURLVolumeNameKey = @"NSURLVolumeNameKey";
NSURLResourceKey const NSURLVolumeLocalizedNameKey = @"NSURLVolumeLocalizedNameKey";
NSURLResourceKey const NSURLVolumeIsEncryptedKey = @"NSURLVolumeIsEncryptedKey";
NSURLResourceKey const NSURLVolumeIsRootFileSystemKey = @"NSURLVolumeIsRootFileSystemKey";
NSURLResourceKey const NSURLVolumeSupportsCompressionKey = @"NSURLVolumeSupportsCompressionKey";
NSURLResourceKey const NSURLVolumeSupportsFileCloningKey = @"NSURLVolumeSupportsFileCloningKey";
NSURLResourceKey const NSURLVolumeSupportsSwapRenamingKey = @"NSURLVolumeSupportsSwapRenamingKey";
NSURLResourceKey const NSURLVolumeSupportsExclusiveRenamingKey = @"NSURLVolumeSupportsExclusiveRenamingKey";
NSURLResourceKey const NSURLVolumeSupportsImmutableFilesKey = @"NSURLVolumeSupportsImmutableFilesKey";
NSURLResourceKey const NSURLVolumeSupportsAccessPermissionsKey = @"NSURLVolumeSupportsAccessPermissionsKey";
NSURLResourceKey const NSURLVolumeAvailableCapacityForImportantUsageKey = @"NSURLVolumeAvailableCapacityForImportantUsageKey";
NSURLResourceKey const NSURLVolumeAvailableCapacityForOpportunisticUsageKey = @"NSURLVolumeAvailableCapacityForOpportunisticUsageKey";
NSURLFileResourceType const NSURLFileResourceTypeNamedPipe = @"NSURLFileResourceTypeNamedPipe";
NSURLFileResourceType const NSURLFileResourceTypeCharacterSpecial = @"NSURLFileResourceTypeCharacterSpecial";
NSURLFileResourceType const NSURLFileResourceTypeDirectory = @"NSURLFileResourceTypeDirectory";
NSURLFileResourceType const NSURLFileResourceTypeBlockSpecial = @"NSURLFileResourceTypeBlockSpecial";
NSURLFileResourceType const NSURLFileResourceTypeRegular = @"NSURLFileResourceTypeRegular";
NSURLFileResourceType const NSURLFileResourceTypeSymbolicLink = @"NSURLFileResourceTypeSymbolicLink";
NSURLFileResourceType const NSURLFileResourceTypeSocket = @"NSURLFileResourceTypeSocket";
NSURLFileResourceType const NSURLFileResourceTypeUnknown = @"NSURLFileResourceTypeUnknown";

static NSString *const tagsAttribute = @"com.apple.metadata:_kMDItemUserTags";
static NSString *const backupAttribute = @"com.apple.metadata:com_apple_backup_excludeItem";

static NSError *fileError(NSInteger code, NSURL *url, int posixError) {
    NSMutableDictionary *info = [NSMutableDictionary dictionary];
    if (url != nil) {
        info[NSURLErrorKey] = url;
        if ([url isFileURL] && [url path])
            info[NSFilePathErrorKey] = [url path];
    }
    if (posixError != 0)
        info[NSUnderlyingErrorKey] = [NSError errorWithDomain:NSPOSIXErrorDomain code:posixError userInfo:nil];
    return [NSError errorWithDomain:NSCocoaErrorDomain code:code userInfo:info];
}

static NSInteger readErrorCode(int error) {
    switch (error) {
    case ENOENT: case ENOTDIR: return NSFileReadNoSuchFileError;
    case EACCES: case EPERM:   return NSFileReadNoPermissionError;
    default:                   return NSFileReadUnknownError;
    }
}

static NSInteger writeErrorCode(int error) {
    switch (error) {
    case ENOENT: case ENOTDIR: return NSFileNoSuchFileError;
    case EACCES: case EPERM:   return NSFileWriteNoPermissionError;
    case EEXIST:               return NSFileWriteFileExistsError;
    case ENOSPC:               return NSFileWriteOutOfSpaceError;
    case EROFS:                return NSFileWriteVolumeReadOnlyError;
    default:                   return NSFileWriteUnknownError;
    }
}

static NSDate *dateFrom(struct timespec t) {
    return [NSDate dateWithTimeIntervalSince1970:(NSTimeInterval)t.tv_sec + t.tv_nsec / 1e9];
}

#pragma mark Types

static NSSet *packageExtensions(void) {
    static NSSet *set;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        set = [NSSet setWithObjects:@"app", @"bundle", @"framework", @"plugin", @"kext", @"pkg", @"mpkg",
               @"rtfd", @"xcodeproj", @"xcworkspace", @"playground", @"prefPane", @"qlgenerator",
               @"mdimporter", @"saver", @"wdgt", @"xpc", @"appex", @"photoslibrary", @"dSYM",
               @"component", @"vst", @"vst3", @"nib", @"xcarchive",
               @"plugInSuite", @"service", @"action", @"workflow", @"scptd", @"systemextension", nil];
    });
    return set;
}

static BOOL isPackagePath(NSString *path, BOOL isDirectory) {
    if (!isDirectory)
        return NO;
    NSString *extension = [path pathExtension];
    if ([extension length] == 0)
        return NO;
    for (NSString *known in packageExtensions())
        if ([known caseInsensitiveCompare:extension] == NSOrderedSame)
            return YES;
    return NO;
}

// The uniform type identifier and description of a file: a table of the
// common extensions.
static NSString *typeIdentifier(NSString *path, struct stat *st, NSString **description) {
    static NSDictionary *types;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        types = @{
            @"txt": @[ @"public.plain-text", @"Plain Text Document" ],
            @"text": @[ @"public.plain-text", @"Plain Text Document" ],
            @"md": @[ @"net.daringfireball.markdown", @"Markdown Document" ],
            @"rtf": @[ @"public.rtf", @"Rich Text Document" ],
            @"html": @[ @"public.html", @"HTML text" ], @"htm": @[ @"public.html", @"HTML text" ],
            @"xml": @[ @"public.xml", @"XML text" ], @"json": @[ @"public.json", @"JSON" ],
            @"plist": @[ @"com.apple.property-list", @"Property List" ],
            @"c": @[ @"public.c-source", @"C Source" ], @"h": @[ @"public.c-header", @"C Header Source" ],
            @"m": @[ @"public.objective-c-source", @"Objective-C Source" ],
            @"swift": @[ @"public.swift-source", @"Swift Source" ],
            @"sh": @[ @"public.shell-script", @"Shell Script" ], @"py": @[ @"public.python-script", @"Python Script" ],
            @"pdf": @[ @"com.adobe.pdf", @"PDF document" ],
            @"png": @[ @"public.png", @"PNG image" ], @"jpg": @[ @"public.jpeg", @"JPEG image" ],
            @"jpeg": @[ @"public.jpeg", @"JPEG image" ], @"gif": @[ @"com.compuserve.gif", @"GIF image" ],
            @"tiff": @[ @"public.tiff", @"TIFF image" ], @"icns": @[ @"com.apple.icns", @"Apple icon image" ],
            @"svg": @[ @"public.svg-image", @"SVG image" ],
            @"zip": @[ @"public.zip-archive", @"ZIP archive" ], @"gz": @[ @"org.gnu.gnu-zip-archive", @"gzip compressed archive" ],
            @"tar": @[ @"public.tar-archive", @"tar archive" ], @"dmg": @[ @"com.apple.disk-image-udif", @"Disk Image" ],
            @"app": @[ @"com.apple.application-bundle", @"Application" ],
            @"framework": @[ @"com.apple.framework", @"Framework" ], @"bundle": @[ @"com.apple.bundle", @"Bundle" ],
            @"dylib": @[ @"com.apple.mach-o-dylib", @"Dynamic Library" ],
            @"mp3": @[ @"public.mp3", @"MP3 audio" ], @"mp4": @[ @"public.mpeg-4", @"MPEG-4 movie" ],
            @"mov": @[ @"com.apple.quicktime-movie", @"QuickTime movie" ],
        };
    });
    if (S_ISDIR(st->st_mode)) {
        NSArray *known = types[[[path pathExtension] lowercaseString]];
        if (known != nil && isPackagePath(path, YES)) {
            *description = known[1];
            return known[0];
        }
        if (isPackagePath(path, YES)) {
            *description = @"Package";
            return @"com.apple.package";
        }
        *description = @"Folder";
        return [path isEqualToString:@"/"] ? @"public.volume" : @"public.folder";
    }
    if (S_ISLNK(st->st_mode)) {
        *description = @"Alias";
        return @"public.symlink";
    }
    NSArray *known = types[[[path pathExtension] lowercaseString]];
    if (known != nil) {
        *description = known[1];
        return known[0];
    }
    if (S_ISREG(st->st_mode) && (st->st_mode & 0111)) {
        *description = @"Unix executable";
        return @"public.unix-executable";
    }
    *description = @"Document";
    return @"public.data";
}

#pragma mark Extended attributes

static NSData *readAttribute(const char *path, NSString *name) {
    ssize_t size = getxattr(path, [name UTF8String], NULL, 0, 0, XATTR_NOFOLLOW);
    if (size < 0)
        return nil;
    NSMutableData *data = [NSMutableData dataWithLength:(NSUInteger)size];
    size = getxattr(path, [name UTF8String], [data mutableBytes], (size_t)size, 0, XATTR_NOFOLLOW);
    if (size < 0)
        return nil;
    [data setLength:(NSUInteger)size];
    return data;
}

// Tags are kept as "Name" or "Name\n<color number>" in a property list.
static NSArray *tagNames(const char *path) {
    NSData *data = readAttribute(path, tagsAttribute);
    NSArray *list = data ? [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL] : nil;
    if (![list isKindOfClass:[NSArray class]])
        return nil;
    NSMutableArray *names = [NSMutableArray array];
    for (NSString *tag in list)
        if ([tag isKindOfClass:[NSString class]])
            [names addObject:[[tag componentsSeparatedByString:@"\n"] firstObject]];
    return [names count] ? names : nil;
}

#pragma mark Volumes

static NSString *volumeName(const char *mountPoint) {
    struct attrlist list;
    struct {
        uint32_t length;
        attrreference_t name;
        char space[MAXPATHLEN + 1];
    } __attribute__((aligned(4), packed)) buffer;
    memset(&list, 0, sizeof(list));
    list.bitmapcount = ATTR_BIT_MAP_COUNT;
    list.volattr = ATTR_VOL_INFO | ATTR_VOL_NAME;
    if (getattrlist(mountPoint, &list, &buffer, sizeof(buffer), 0) == 0 && buffer.name.attr_length > 1)
        return [NSString stringWithUTF8String:(const char *)&buffer.name + buffer.name.attr_dataoffset];
    NSString *name = [[NSString stringWithUTF8String:mountPoint] lastPathComponent];
    return [name isEqualToString:@"/"] ? @"Minkar" : name;
}

static NSString *formatDescription(const char *type) {
    if (strcmp(type, "hfs") == 0) return @"Mac OS Extended";
    if (strcmp(type, "apfs") == 0) return @"APFS";
    if (strcmp(type, "msdos") == 0) return @"MS-DOS (FAT)";
    if (strcmp(type, "exfat") == 0) return @"ExFAT";
    if (strcmp(type, "nfs") == 0) return @"NFS";
    if (strcmp(type, "devfs") == 0) return @"devfs";
    return [NSString stringWithUTF8String:type];
}

#pragma mark Values

// The value of a key for the file at path (lstat'ed into st): nil when it
// has none.
static id resourceValue(NSURL *url __attribute__((unused)), NSString *path, const char *fs, struct stat *st, NSString *key) {
    BOOL directory = S_ISDIR(st->st_mode);
    if ([key isEqualToString:NSURLNameKey]) return [path isEqualToString:@"/"] ? volumeName("/") : [path lastPathComponent];
    if ([key isEqualToString:NSURLLocalizedNameKey]) {
        if ([path isEqualToString:@"/"]) return volumeName("/");
        NSString *name = [path lastPathComponent];
        return [[[name pathExtension] lowercaseString] isEqualToString:@"app"] ? [name stringByDeletingPathExtension] : name;
    }
    if ([key isEqualToString:NSURLPathKey]) return path;
    if ([key isEqualToString:NSURLCanonicalPathKey]) {
        char resolved[PATH_MAX];
        return realpath(fs, resolved) ? [NSString stringWithUTF8String:resolved] : path;
    }
    if ([key isEqualToString:NSURLIsRegularFileKey]) return @(S_ISREG(st->st_mode));
    if ([key isEqualToString:NSURLIsDirectoryKey]) return @(directory);
    if ([key isEqualToString:NSURLIsSymbolicLinkKey]) return @(S_ISLNK(st->st_mode));
    if ([key isEqualToString:NSURLIsAliasFileKey]) return @(S_ISLNK(st->st_mode));
    if ([key isEqualToString:NSURLIsPackageKey]) return @(isPackagePath(path, directory));
    if ([key isEqualToString:NSURLIsApplicationKey])
        return @(directory && [[[path pathExtension] lowercaseString] isEqualToString:@"app"]);
    if ([key isEqualToString:NSURLApplicationIsScriptableKey]) {
        NSDictionary *info = [NSDictionary dictionaryWithContentsOfFile:[path stringByAppendingPathComponent:@"Contents/Info.plist"]];
        return @([info[@"NSAppleScriptEnabled"] boolValue]);
    }
    if ([key isEqualToString:NSURLIsVolumeKey]) {
        struct statfs fsinfo;
        return @(statfs(fs, &fsinfo) == 0 && strcmp(fsinfo.f_mntonname, fs) == 0);
    }
    if ([key isEqualToString:NSURLIsHiddenKey])
        return @([[path lastPathComponent] hasPrefix:@"."] || (st->st_flags & UF_HIDDEN) != 0);
    if ([key isEqualToString:NSURLHasHiddenExtensionKey]) return @NO;
    if ([key isEqualToString:NSURLIsUserImmutableKey]) return @((st->st_flags & UF_IMMUTABLE) != 0);
    if ([key isEqualToString:NSURLIsSystemImmutableKey]) return @((st->st_flags & SF_IMMUTABLE) != 0);
    if ([key isEqualToString:NSURLIsReadableKey]) return @(access(fs, R_OK) == 0);
    if ([key isEqualToString:NSURLIsWritableKey]) return @(access(fs, W_OK) == 0);
    if ([key isEqualToString:NSURLIsExecutableKey]) return @(access(fs, X_OK) == 0);
    if ([key isEqualToString:NSURLIsMountTriggerKey]) return @NO;
    if ([key isEqualToString:NSURLCreationDateKey]) return dateFrom(st->st_birthtimespec);
    if ([key isEqualToString:NSURLContentModificationDateKey]) return dateFrom(st->st_mtimespec);
    if ([key isEqualToString:NSURLContentAccessDateKey]) return dateFrom(st->st_atimespec);
    if ([key isEqualToString:NSURLAttributeModificationDateKey]) return dateFrom(st->st_ctimespec);
    if ([key isEqualToString:NSURLLinkCountKey]) return @(st->st_nlink);
    if ([key isEqualToString:NSURLPreferredIOBlockSizeKey]) return @(st->st_blksize);
    if ([key isEqualToString:NSURLLabelNumberKey]) return @0;
    if ([key isEqualToString:NSURLParentDirectoryURLKey])
        return [path isEqualToString:@"/"] ? nil : [NSURL fileURLWithPath:[path stringByDeletingLastPathComponent] isDirectory:YES];
    if ([key isEqualToString:NSURLFileResourceIdentifierKey]) {
        uint64_t identity[2] = { (uint64_t)st->st_dev, (uint64_t)st->st_ino };
        return [NSData dataWithBytes:identity length:sizeof(identity)];
    }
    if ([key isEqualToString:NSURLFileResourceTypeKey]) {
        switch (st->st_mode & S_IFMT) {
        case S_IFIFO: return NSURLFileResourceTypeNamedPipe;
        case S_IFCHR: return NSURLFileResourceTypeCharacterSpecial;
        case S_IFDIR: return NSURLFileResourceTypeDirectory;
        case S_IFBLK: return NSURLFileResourceTypeBlockSpecial;
        case S_IFREG: return NSURLFileResourceTypeRegular;
        case S_IFLNK: return NSURLFileResourceTypeSymbolicLink;
        case S_IFSOCK: return NSURLFileResourceTypeSocket;
        default: return NSURLFileResourceTypeUnknown;
        }
    }
    if ([key isEqualToString:NSURLFileSizeKey] || [key isEqualToString:NSURLTotalFileSizeKey])
        return S_ISREG(st->st_mode) ? @(st->st_size) : nil;
    if ([key isEqualToString:NSURLFileAllocatedSizeKey] || [key isEqualToString:NSURLTotalFileAllocatedSizeKey])
        return S_ISREG(st->st_mode) ? @((long long)st->st_blocks * 512) : nil;
    if ([key isEqualToString:NSURLTypeIdentifierKey] || [key isEqualToString:NSURLLocalizedTypeDescriptionKey]) {
        NSString *description = nil;
        NSString *type = typeIdentifier(path, st, &description);
        return [key isEqualToString:NSURLTypeIdentifierKey] ? type : description;
    }
    if ([key isEqualToString:NSURLTagNamesKey]) return tagNames(fs);
    if ([key isEqualToString:NSURLIsExcludedFromBackupKey]) return @(readAttribute(fs, backupAttribute) != nil);

    if ([key hasPrefix:@"NSURLVolume"]) {
        struct statfs v;
        if (statfs(fs, &v) != 0)
            return nil;
        if ([key isEqualToString:NSURLVolumeURLKey]) return [NSURL fileURLWithPath:[NSString stringWithUTF8String:v.f_mntonname] isDirectory:YES];
        if ([key isEqualToString:NSURLVolumeNameKey] || [key isEqualToString:NSURLVolumeLocalizedNameKey]) return volumeName(v.f_mntonname);
        if ([key isEqualToString:NSURLVolumeIdentifierKey]) return [NSData dataWithBytes:&v.f_fsid length:sizeof(v.f_fsid)];
        if ([key isEqualToString:NSURLVolumeLocalizedFormatDescriptionKey]) return formatDescription(v.f_fstypename);
        if ([key isEqualToString:NSURLVolumeTotalCapacityKey]) return @((long long)v.f_blocks * v.f_bsize);
        if ([key isEqualToString:NSURLVolumeAvailableCapacityKey] ||
            [key isEqualToString:NSURLVolumeAvailableCapacityForImportantUsageKey] ||
            [key isEqualToString:NSURLVolumeAvailableCapacityForOpportunisticUsageKey])
            return @((long long)v.f_bavail * v.f_bsize);
        if ([key isEqualToString:NSURLVolumeResourceCountKey]) return @((long long)(v.f_files - v.f_ffree));
        if ([key isEqualToString:NSURLVolumeIsLocalKey]) return @((v.f_flags & MNT_LOCAL) != 0);
        if ([key isEqualToString:NSURLVolumeIsReadOnlyKey]) return @((v.f_flags & MNT_RDONLY) != 0);
        if ([key isEqualToString:NSURLVolumeIsRootFileSystemKey]) return @((v.f_flags & MNT_ROOTFS) != 0 || strcmp(v.f_mntonname, "/") == 0);
        if ([key isEqualToString:NSURLVolumeIsRemovableKey] || [key isEqualToString:NSURLVolumeIsEjectableKey])
            return @((v.f_flags & MNT_REMOVABLE) != 0);
        if ([key isEqualToString:NSURLVolumeIsInternalKey]) return @((v.f_flags & MNT_LOCAL) != 0 && (v.f_flags & MNT_REMOVABLE) == 0);
        if ([key isEqualToString:NSURLVolumeIsAutomountedKey]) return @((v.f_flags & MNT_AUTOMOUNTED) != 0);
        if ([key isEqualToString:NSURLVolumeIsBrowsableKey]) return @((v.f_flags & MNT_DONTBROWSE) == 0);
        if ([key isEqualToString:NSURLVolumeIsJournalingKey]) return @((v.f_flags & MNT_JOURNALED) != 0);
        if ([key isEqualToString:NSURLVolumeIsEncryptedKey]) return @((v.f_flags & MNT_CPROTECT) != 0);
        if ([key isEqualToString:NSURLVolumeSupportsSymbolicLinksKey] ||
            [key isEqualToString:NSURLVolumeSupportsHardLinksKey] ||
            [key isEqualToString:NSURLVolumeSupportsCasePreservedNamesKey] ||
            [key isEqualToString:NSURLVolumeSupportsPersistentIDsKey] ||
            [key isEqualToString:NSURLVolumeSupportsVolumeSizesKey] ||
            [key isEqualToString:NSURLVolumeSupportsRenamingKey] ||
            [key isEqualToString:NSURLVolumeSupportsAdvisoryFileLockingKey] ||
            [key isEqualToString:NSURLVolumeSupportsAccessPermissionsKey])
            return @(strcmp(v.f_fstypename, "msdos") != 0 && strcmp(v.f_fstypename, "exfat") != 0);
        if ([key isEqualToString:NSURLVolumeSupportsCaseSensitiveNamesKey]) {
            long sensitive = pathconf(v.f_mntonname, _PC_CASE_SENSITIVE);
            return @(sensitive == 1);
        }
        if ([key isEqualToString:NSURLVolumeMaximumFileSizeKey]) {
            long bits = pathconf(v.f_mntonname, _PC_FILESIZEBITS);
            return bits > 0 && bits < 64 ? @((1LL << bits) - 1) : @(LLONG_MAX);
        }
        if ([key isEqualToString:NSURLVolumeURLForRemountingKey]) return nil;
        return @NO;   // the other volume capabilities
    }
    return nil;
}

@implementation NSURL (NSURLResources)

+ (NSURL *)fileURLWithPath:(NSString *)path relativeToURL:(NSURL *)baseURL {
    return [[self alloc] initFileURLWithPath:path relativeToURL:baseURL];
}

+ (NSURL *)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL {
    return [[self alloc] initFileURLWithPath:path isDirectory:isDir relativeToURL:baseURL];
}

+ (NSURL *)fileURLWithFileSystemRepresentation:(const char *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL {
    return [[self alloc] initFileURLWithFileSystemRepresentation:path isDirectory:isDir relativeToURL:baseURL];
}

// A relative path is taken from the base URL's path (not the current
// directory); the URL made is absolute.
- (instancetype)initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL {
    if (baseURL != nil && ![path hasPrefix:@"/"]) {
        NSString *base = [baseURL path];
        if (![baseURL hasDirectoryPath])
            base = [base stringByDeletingLastPathComponent];
        path = [base stringByAppendingPathComponent:path];
    }
    return [self initFileURLWithPath:path isDirectory:isDir];
}

- (instancetype)initFileURLWithPath:(NSString *)path relativeToURL:(NSURL *)baseURL {
    if (baseURL != nil && ![path hasPrefix:@"/"]) {
        NSString *base = [baseURL path];
        if (![baseURL hasDirectoryPath])
            base = [base stringByDeletingLastPathComponent];
        path = [base stringByAppendingPathComponent:path];
    }
    return [self initFileURLWithPath:path];
}

- (instancetype)initFileURLWithFileSystemRepresentation:(const char *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL {
    NSString *string = path ? [[NSFileManager defaultManager] stringWithFileSystemRepresentation:path length:strlen(path)] : nil;
    return [self initFileURLWithPath:string isDirectory:isDir relativeToURL:baseURL];
}

- (BOOL)getFileSystemRepresentation:(char *)buffer maxLength:(NSUInteger)maxBufferLength {
    NSString *path = [self isFileURL] ? [self path] : nil;
    const char *fs = [path fileSystemRepresentation];
    if (fs == NULL || strlen(fs) + 1 > maxBufferLength)
        return NO;
    memcpy(buffer, fs, strlen(fs) + 1);
    return YES;
}

- (const char *)fileSystemRepresentation {
    return [self isFileURL] ? [[self path] fileSystemRepresentation] : NULL;
}

- (BOOL)isFileReferenceURL {
    return NO;
}

- (NSURL *)fileReferenceURL {
    return [self isFileURL] ? self : nil;
}

- (NSURL *)filePathURL {
    return [self isFileURL] ? self : nil;
}

// NO, with the error, when the file is not there (or not a file URL).
- (BOOL)_stat:(struct stat *)st error:(NSError **)error {
    if (![self isFileURL]) {
        if (error) *error = fileError(NSFileReadUnsupportedSchemeError, self, 0);
        return NO;
    }
    if (lstat([[self path] fileSystemRepresentation], st) != 0) {
        int e = errno;
        if (error) *error = fileError(readErrorCode(e), self, e);
        return NO;
    }
    return YES;
}

- (BOOL)checkResourceIsReachableAndReturnError:(NSError **)error {
    struct stat st;
    return [self _stat:&st error:error];
}

- (BOOL)getResourceValue:(id *)value forKey:(NSString *)key error:(NSError **)error {
    struct stat st;
    if (value) *value = nil;
    if (![self _stat:&st error:error])
        return NO;
    NSString *path = [self path];
    id result = resourceValue(self, path, [path fileSystemRepresentation], &st, key);
    if (value) *value = result;
    return YES;
}

- (NSDictionary *)resourceValuesForKeys:(NSArray *)keys error:(NSError **)error {
    struct stat st;
    if (![self _stat:&st error:error])
        return nil;
    NSString *path = [self path];
    const char *fs = [path fileSystemRepresentation];
    NSMutableDictionary *values = [NSMutableDictionary dictionary];
    for (NSString *key in keys) {
        id value = resourceValue(self, path, fs, &st, key);
        if (value != nil)
            values[key] = value;
    }
    return values;
}

static BOOL setTimes(const char *fs, NSDate *date, BOOL access) {
    struct stat st;
    if (stat(fs, &st) != 0)
        return NO;
    struct timeval times[2];
    TIMESPEC_TO_TIMEVAL(&times[0], &st.st_atimespec);
    TIMESPEC_TO_TIMEVAL(&times[1], &st.st_mtimespec);
    NSTimeInterval t = [date timeIntervalSince1970];
    struct timeval *changed = access ? &times[0] : &times[1];
    changed->tv_sec = (time_t)t;
    changed->tv_usec = (suseconds_t)((t - (time_t)t) * 1e6);
    return utimes(fs, times) == 0;
}

static BOOL setCreationDate(const char *fs, NSDate *date) {
    struct attrlist list;
    struct timespec ts;
    NSTimeInterval t = [date timeIntervalSince1970];
    memset(&list, 0, sizeof(list));
    list.bitmapcount = ATTR_BIT_MAP_COUNT;
    list.commonattr = ATTR_CMN_CRTIME;
    ts.tv_sec = (time_t)t;
    ts.tv_nsec = (long)((t - (time_t)t) * 1e9);
    return setattrlist(fs, &list, &ts, sizeof(ts), 0) == 0;
}

static BOOL setFlag(const char *fs, uint32_t flag, BOOL on) {
    struct stat st;
    if (lstat(fs, &st) != 0)
        return NO;
    uint32_t flags = on ? (st.st_flags | flag) : (st.st_flags & ~flag);
    return flags == st.st_flags || lchflags(fs, flags) == 0;
}

- (BOOL)setResourceValue:(id)value forKey:(NSString *)key error:(NSError **)error {
    struct stat st;
    if (![self _stat:&st error:error])
        return NO;
    const char *fs = [[self path] fileSystemRepresentation];
    BOOL ok = YES;
    if ([key isEqualToString:NSURLContentModificationDateKey] || [key isEqualToString:NSURLContentAccessDateKey]) {
        ok = [value isKindOfClass:[NSDate class]] && setTimes(fs, value, [key isEqualToString:NSURLContentAccessDateKey]);
    } else if ([key isEqualToString:NSURLCreationDateKey]) {
        ok = [value isKindOfClass:[NSDate class]] && setCreationDate(fs, value);
    } else if ([key isEqualToString:NSURLIsHiddenKey]) {
        ok = setFlag(fs, UF_HIDDEN, [value boolValue]);
    } else if ([key isEqualToString:NSURLIsUserImmutableKey]) {
        ok = setFlag(fs, UF_IMMUTABLE, [value boolValue]);
    } else if ([key isEqualToString:NSURLIsExcludedFromBackupKey]) {
        if ([value boolValue]) {
            const char yes[] = "com.apple.backupd";
            ok = setxattr(fs, [backupAttribute UTF8String], yes, sizeof(yes) - 1, 0, 0) == 0;
        } else {
            ok = removexattr(fs, [backupAttribute UTF8String], 0) == 0 || errno == ENOATTR;
        }
    } else if ([key isEqualToString:NSURLTagNamesKey]) {
        NSArray *tags = [value isKindOfClass:[NSArray class]] ? value : @[];
        if ([tags count] == 0) {
            ok = removexattr(fs, [tagsAttribute UTF8String], 0) == 0 || errno == ENOATTR;
        } else {
            NSData *data = [NSPropertyListSerialization dataWithPropertyList:tags format:NSPropertyListBinaryFormat_v1_0
                                                                     options:0 error:NULL];
            ok = data && setxattr(fs, [tagsAttribute UTF8String], [data bytes], [data length], 0, 0) == 0;
        }
    } else if ([key isEqualToString:NSURLHasHiddenExtensionKey] || [key isEqualToString:NSURLLabelNumberKey]) {
        ok = YES;   // nothing keeps them
    } else {
        if (error) *error = fileError(NSFileWriteUnknownError, self, 0);
        return NO;
    }
    if (!ok && error)
        *error = fileError(writeErrorCode(errno), self, errno);
    return ok;
}

- (BOOL)setResourceValues:(NSDictionary *)keyedValues error:(NSError **)error {
    for (NSString *key in keyedValues)
        if (![self setResourceValue:keyedValues[key] forKey:key error:error])
            return NO;
    return YES;
}

// Nothing is cached.
- (void)removeCachedResourceValueForKey:(NSString *)key {
}

- (void)removeAllCachedResourceValues {
}

- (void)setTemporaryResourceValue:(id)value forKey:(NSString *)key {
}

#pragma mark Bookmarks

static NSString *const bookmarkMagic = @"org.minkar.bookmark";

- (NSData *)bookmarkDataWithOptions:(NSURLBookmarkCreationOptions)options
     includingResourceValuesForKeys:(NSArray *)keys relativeToURL:(NSURL *)relativeURL error:(NSError **)error {
    struct stat st;
    if (![self _stat:&st error:error])
        return nil;
    NSMutableDictionary *bookmark = [NSMutableDictionary dictionary];
    bookmark[@"Kind"] = bookmarkMagic;
    bookmark[@"Version"] = @1;
    bookmark[@"Path"] = [self path];
    bookmark[@"Device"] = @((long long)st.st_dev);
    bookmark[@"Inode"] = @((long long)st.st_ino);
    bookmark[@"Directory"] = @(S_ISDIR(st.st_mode));
    if (relativeURL != nil && [relativeURL isFileURL])
        bookmark[@"RelativeTo"] = [relativeURL path];
    if ([keys count] > 0) {
        NSMutableDictionary *values = [NSMutableDictionary dictionary];
        NSDictionary *all = [self resourceValuesForKeys:keys error:NULL];
        for (NSString *key in all) {
            id value = all[key];
            if ([value isKindOfClass:[NSURL class]])
                value = [value absoluteString];
            if ([value isKindOfClass:[NSString class]] || [value isKindOfClass:[NSNumber class]] ||
                [value isKindOfClass:[NSDate class]] || [value isKindOfClass:[NSData class]] ||
                [value isKindOfClass:[NSArray class]])
                values[key] = value;
        }
        bookmark[@"Values"] = values;
    }
    return [NSPropertyListSerialization dataWithPropertyList:bookmark format:NSPropertyListBinaryFormat_v1_0
                                                     options:0 error:error];
}

static NSDictionary *readBookmark(NSData *data) {
    NSDictionary *bookmark = data ? [NSPropertyListSerialization propertyListWithData:data options:0 format:NULL error:NULL] : nil;
    if (![bookmark isKindOfClass:[NSDictionary class]] || ![bookmark[@"Kind"] isEqual:bookmarkMagic])
        return nil;
    return bookmark;
}

// The path if the file is there; stale when another file has its place.
- (instancetype)initByResolvingBookmarkData:(NSData *)bookmarkData options:(NSURLBookmarkResolutionOptions)options
                              relativeToURL:(NSURL *)relativeURL bookmarkDataIsStale:(BOOL *)isStale
                                      error:(NSError **)error {
    NSDictionary *bookmark = readBookmark(bookmarkData);
    if (isStale) *isStale = NO;
    if (bookmark == nil) {
        if (error) *error = fileError(NSFileReadCorruptFileError, nil, 0);
        return nil;
    }
    NSString *path = bookmark[@"Path"];
    struct stat st;
    if (lstat([path fileSystemRepresentation], &st) != 0) {
        int e = errno;
        if (error) *error = fileError(NSFileNoSuchFileError, [NSURL fileURLWithPath:path], e);
        return nil;
    }
    if (isStale && ((long long)st.st_ino != [bookmark[@"Inode"] longLongValue] ||
                    (long long)st.st_dev != [bookmark[@"Device"] longLongValue]))
        *isStale = YES;
    return [self initFileURLWithPath:path isDirectory:S_ISDIR(st.st_mode)];
}

+ (instancetype)URLByResolvingBookmarkData:(NSData *)bookmarkData options:(NSURLBookmarkResolutionOptions)options
                             relativeToURL:(NSURL *)relativeURL bookmarkDataIsStale:(BOOL *)isStale
                                     error:(NSError **)error {
    return [[self alloc] initByResolvingBookmarkData:bookmarkData options:options relativeToURL:relativeURL
                                 bookmarkDataIsStale:isStale error:error];
}

+ (NSDictionary *)resourceValuesForKeys:(NSArray *)keys fromBookmarkData:(NSData *)bookmarkData {
    NSDictionary *bookmark = readBookmark(bookmarkData);
    if (bookmark == nil)
        return nil;
    NSMutableDictionary *result = [NSMutableDictionary dictionary];
    NSDictionary *values = bookmark[@"Values"];
    for (NSString *key in keys) {
        if (values[key] != nil)
            result[key] = values[key];
        else if ([key isEqualToString:NSURLPathKey])
            result[key] = bookmark[@"Path"];
        else if ([key isEqualToString:NSURLNameKey])
            result[key] = [bookmark[@"Path"] lastPathComponent];
        else if ([key isEqualToString:NSURLIsDirectoryKey])
            result[key] = bookmark[@"Directory"];
    }
    return result;
}

+ (BOOL)writeBookmarkData:(NSData *)bookmarkData toURL:(NSURL *)bookmarkFileURL
                  options:(NSURLBookmarkFileCreationOptions)options error:(NSError **)error {
    return [bookmarkData writeToURL:bookmarkFileURL options:NSDataWritingAtomic error:error];
}

+ (NSData *)bookmarkDataWithContentsOfURL:(NSURL *)bookmarkFileURL error:(NSError **)error {
    NSData *data = [NSData dataWithContentsOfURL:bookmarkFileURL options:0 error:error];
    if (data != nil && readBookmark(data) == nil) {
        if (error) *error = fileError(NSFileReadCorruptFileError, bookmarkFileURL, 0);
        return nil;
    }
    return data;
}

// Aliases are symbolic links here.
+ (instancetype)URLByResolvingAliasFileAtURL:(NSURL *)url options:(NSURLBookmarkResolutionOptions)options
                                       error:(NSError **)error {
    struct stat st;
    if (![url _stat:&st error:error])
        return nil;
    if (!S_ISLNK(st.st_mode))
        return url;
    return [url URLByResolvingSymlinksInPath];
}

- (BOOL)startAccessingSecurityScopedResource {
    return YES;
}

- (void)stopAccessingSecurityScopedResource {
}

@end

#pragma mark - NSFileManager

// A directory enumerator that gives URLs, depth first.
@interface _NSURLDirectoryEnumerator : NSDirectoryEnumerator {
@public
    NSMutableArray *_stack;          // of @[ path, names, next index ]
    NSDirectoryEnumerationOptions _options;
    BOOL (^_errorHandler)(NSURL *, NSError *);
    NSURL *_base;
    NSString *_lastPath;
    BOOL _lastWasDirectory;
    NSMutableArray *_postOrder;      // directories to give back after their contents
    BOOL _postOrderNow;
}
@end

@implementation _NSURLDirectoryEnumerator

- (BOOL)_push:(NSString *)path {
    NSError *error = nil;
    NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:&error];
    if (names == nil) {
        if (_errorHandler != nil && !_errorHandler([NSURL fileURLWithPath:path isDirectory:YES], error)) {
            [_stack removeAllObjects];
            return NO;
        }
        return YES;
    }
    [_stack addObject:[NSMutableArray arrayWithObjects:path, names, @0, nil]];
    return YES;
}

- (id)nextObject {
    // Descend into the directory given last, unless it was skipped.
    if (_lastWasDirectory && _lastPath != nil) {
        _lastWasDirectory = NO;
        if ((_options & NSDirectoryEnumerationIncludesDirectoriesPostOrder) != 0)
            [_postOrder addObject:_lastPath];
        [self _push:_lastPath];
    }
    _postOrderNow = NO;
    while ([_stack count] > 0) {
        NSMutableArray *top = [_stack lastObject];
        NSArray *names = top[1];
        NSUInteger index = [top[2] unsignedIntegerValue];
        if (index >= [names count]) {
            [_stack removeLastObject];
            if ([_postOrder count] > 0 && [[_postOrder lastObject] isEqualToString:top[0]]) {
                NSString *done = [_postOrder lastObject];
                [_postOrder removeLastObject];
                _postOrderNow = YES;
                _lastPath = nil;
                return [NSURL fileURLWithPath:done isDirectory:YES];
            }
            continue;
        }
        top[2] = @(index + 1);
        NSString *name = names[index];
        if ((_options & NSDirectoryEnumerationSkipsHiddenFiles) && [name hasPrefix:@"."])
            continue;
        NSString *path = [top[0] stringByAppendingPathComponent:name];
        struct stat st;
        BOOL directory = lstat([path fileSystemRepresentation], &st) == 0 && S_ISDIR(st.st_mode);
        BOOL descend = directory && !(_options & NSDirectoryEnumerationSkipsSubdirectoryDescendants) &&
                       !((_options & NSDirectoryEnumerationSkipsPackageDescendants) && isPackagePath(path, YES));
        _lastPath = path;
        _lastWasDirectory = descend;
        if ((_options & NSDirectoryEnumerationProducesRelativePathURLs) && _base != nil) {
            NSString *relative = [path substringFromIndex:[[_base path] length] + 1];
            return [NSURL fileURLWithPath:relative isDirectory:directory relativeToURL:_base];
        }
        return [NSURL fileURLWithPath:path isDirectory:directory];
    }
    return nil;
}

- (void)skipDescendants {
    _lastWasDirectory = NO;
}

- (void)skipDescendents {
    [self skipDescendants];
}

- (NSUInteger)level {
    return [_stack count];
}

- (BOOL)isEnumeratingDirectoryPostOrder {
    return _postOrderNow;
}

- (NSDictionary *)fileAttributes {
    return _lastPath ? [[NSFileManager defaultManager] attributesOfItemAtPath:_lastPath error:NULL] : nil;
}

- (NSDictionary *)directoryAttributes {
    NSString *path = [[_stack firstObject] firstObject];
    return path ? [[NSFileManager defaultManager] attributesOfItemAtPath:path error:NULL] : nil;
}

@end

@implementation NSFileManager (NSURLResources)

- (NSArray *)URLsForDirectory:(NSSearchPathDirectory)directory inDomains:(NSSearchPathDomainMask)domainMask {
    NSMutableArray *urls = [NSMutableArray array];
    for (NSString *path in NSSearchPathForDirectoriesInDomains(directory, domainMask, YES))
        [urls addObject:[NSURL fileURLWithPath:path isDirectory:YES]];
    return urls;
}

// The item replacement directory is a new temporary directory for the
// volume of url; the others are the domain's directory, made if asked.
- (NSURL *)URLForDirectory:(NSSearchPathDirectory)directory inDomain:(NSSearchPathDomainMask)domain
         appropriateForURL:(NSURL *)url create:(BOOL)shouldCreate error:(NSError **)error {
    if (directory == NSItemReplacementDirectory) {
        NSString *app = [[NSProcessInfo processInfo] processName] ?: @"Minkar";
        NSString *base = [NSTemporaryDirectory() stringByAppendingPathComponent:@"TemporaryItems"];
        NSString *template = [base stringByAppendingPathComponent:
                                  [NSString stringWithFormat:@"(A Document Being Saved By %@ XXXXXX)", app]];
        [self createDirectoryAtPath:base withIntermediateDirectories:YES attributes:nil error:NULL];
        char path[PATH_MAX];
        strlcpy(path, [template fileSystemRepresentation], sizeof(path));
        // mkdtemp wants the Xs at the end.
        char *x = strstr(path, "XXXXXX)");
        char suffix[2] = ")";
        if (x != NULL)
            x[6] = '\0';
        if (mkdtemp(path) == NULL) {
            if (error) *error = fileError(writeErrorCode(errno), url, errno);
            return nil;
        }
        NSString *made = [NSString stringWithUTF8String:path];
        NSString *named = [made stringByAppendingString:[NSString stringWithUTF8String:suffix]];
        if (rename(path, [named fileSystemRepresentation]) == 0)
            made = named;
        return [NSURL fileURLWithPath:made isDirectory:YES];
    }
    NSArray *urls = [self URLsForDirectory:directory inDomains:domain];
    NSURL *result = [urls firstObject];
    if (result == nil) {
        if (error) *error = fileError(NSFileReadUnknownError, nil, 0);
        return nil;
    }
    if (shouldCreate && ![self createDirectoryAtURL:result withIntermediateDirectories:YES attributes:nil error:error])
        return nil;
    return result;
}

- (NSURL *)homeDirectoryForCurrentUser {
    return [NSURL fileURLWithPath:NSHomeDirectory() isDirectory:YES];
}

- (NSURL *)homeDirectoryForUser:(NSString *)userName {
    struct passwd *pw = userName ? getpwnam([userName UTF8String]) : NULL;
    return pw ? [NSURL fileURLWithPath:[NSString stringWithUTF8String:pw->pw_dir] isDirectory:YES] : nil;
}

- (NSURL *)temporaryDirectory {
    return [NSURL fileURLWithPath:NSTemporaryDirectory() isDirectory:YES];
}

- (NSArray *)contentsOfDirectoryAtURL:(NSURL *)url includingPropertiesForKeys:(NSArray *)keys
                              options:(NSDirectoryEnumerationOptions)mask error:(NSError **)error {
    NSString *path = [url path];
    NSArray *names = [self contentsOfDirectoryAtPath:path error:error];
    if (names == nil)
        return nil;
    NSMutableArray *urls = [NSMutableArray arrayWithCapacity:[names count]];
    for (NSString *name in names) {
        if ((mask & NSDirectoryEnumerationSkipsHiddenFiles) && [name hasPrefix:@"."])
            continue;
        NSString *item = [path stringByAppendingPathComponent:name];
        struct stat st;
        BOOL directory = lstat([item fileSystemRepresentation], &st) == 0 && S_ISDIR(st.st_mode);
        [urls addObject:[NSURL fileURLWithPath:item isDirectory:directory]];
    }
    return urls;
}

- (NSDirectoryEnumerator *)enumeratorAtURL:(NSURL *)url includingPropertiesForKeys:(NSArray *)keys
                                   options:(NSDirectoryEnumerationOptions)mask
                              errorHandler:(BOOL (^)(NSURL *, NSError *))handler {
    _NSURLDirectoryEnumerator *enumerator = [_NSURLDirectoryEnumerator new];
    enumerator->_stack = [NSMutableArray array];
    enumerator->_postOrder = [NSMutableArray array];
    enumerator->_options = mask;
    enumerator->_errorHandler = [handler copy];
    enumerator->_base = url;
    if (![enumerator _push:[url path]] || [enumerator->_stack count] == 0)
        return enumerator;   // gives nothing
    return enumerator;
}

// The new item takes the original's place; with a backup name the
// original is kept under that name while it happens (and after, if asked).
- (BOOL)replaceItemAtURL:(NSURL *)originalItemURL withItemAtURL:(NSURL *)newItemURL
          backupItemName:(NSString *)backupItemName options:(NSFileManagerItemReplacementOptions)options
        resultingItemURL:(NSURL **)resultingURL error:(NSError **)error {
    const char *original = [[originalItemURL path] fileSystemRepresentation];
    const char *replacement = [[newItemURL path] fileSystemRepresentation];
    NSString *backup = backupItemName ? [[[originalItemURL path] stringByDeletingLastPathComponent]
                                            stringByAppendingPathComponent:backupItemName] : nil;
    struct stat st;
    BOOL exists = lstat(original, &st) == 0;
    if (backup != nil && exists && rename(original, [backup fileSystemRepresentation]) != 0) {
        if (error) *error = fileError(writeErrorCode(errno), originalItemURL, errno);
        return NO;
    }
    if (rename(replacement, original) != 0) {
        int e = errno;
        if (backup != nil && exists)
            rename([backup fileSystemRepresentation], original);
        if (error) *error = fileError(writeErrorCode(e), originalItemURL, e);
        return NO;
    }
    if (backup != nil && exists && !(options & NSFileManagerItemReplacementWithoutDeletingBackupItem))
        [self removeItemAtPath:backup error:NULL];
    if (resultingURL)
        *resultingURL = originalItemURL;
    return YES;
}

// The user's Trash: ~/.Trash, with "name 2", "name 3"... when taken.
- (BOOL)trashItemAtURL:(NSURL *)url resultingItemURL:(NSURL **)outResultingURL error:(NSError **)error {
    NSString *trash = [NSHomeDirectory() stringByAppendingPathComponent:@".Trash"];
    [self createDirectoryAtPath:trash withIntermediateDirectories:YES attributes:nil error:NULL];
    NSString *name = [[url path] lastPathComponent];
    NSString *target = [trash stringByAppendingPathComponent:name];
    for (int n = 2; [self fileExistsAtPath:target]; n++) {
        NSString *extension = [name pathExtension];
        NSString *stem = [name stringByDeletingPathExtension];
        NSString *numbered = [NSString stringWithFormat:@"%@ %d", stem, n];
        target = [trash stringByAppendingPathComponent:[extension length] ? [numbered stringByAppendingPathExtension:extension] : numbered];
    }
    if (rename([[url path] fileSystemRepresentation], [target fileSystemRepresentation]) != 0) {
        int e = errno;
        if (e != EXDEV || ![self moveItemAtPath:[url path] toPath:target error:NULL]) {
            if (error) *error = fileError(writeErrorCode(e), url, e);
            return NO;
        }
    }
    if (outResultingURL)
        *outResultingURL = [NSURL fileURLWithPath:target];
    return YES;
}

- (BOOL)createSymbolicLinkAtURL:(NSURL *)url withDestinationURL:(NSURL *)destURL error:(NSError **)error {
    NSString *destination = [destURL isFileURL] ? [destURL path] : [destURL relativeString];
    if (symlink([destination fileSystemRepresentation], [[url path] fileSystemRepresentation]) != 0) {
        if (error) *error = fileError(writeErrorCode(errno), url, errno);
        return NO;
    }
    return YES;
}

- (BOOL)getRelationship:(NSURLRelationship *)outRelationship ofDirectoryAtURL:(NSURL *)directoryURL
           toItemAtURL:(NSURL *)otherURL error:(NSError **)error {
    NSString *directory = [[directoryURL path] stringByStandardizingPath];
    NSString *other = [[otherURL path] stringByStandardizingPath];
    if (directory == nil || other == nil) {
        if (error) *error = fileError(NSFileReadUnsupportedSchemeError, directoryURL, 0);
        return NO;
    }
    NSURLRelationship relationship = NSURLRelationshipOther;
    if ([directory isEqualToString:other])
        relationship = NSURLRelationshipSame;
    else if ([other hasPrefix:[directory hasSuffix:@"/"] ? directory : [directory stringByAppendingString:@"/"]])
        relationship = NSURLRelationshipContains;
    if (outRelationship)
        *outRelationship = relationship;
    return YES;
}

- (BOOL)getRelationship:(NSURLRelationship *)outRelationship ofDirectory:(NSSearchPathDirectory)directory
              inDomain:(NSSearchPathDomainMask)domainMask toItemAtURL:(NSURL *)url error:(NSError **)error {
    NSArray *urls = [self URLsForDirectory:directory inDomains:domainMask ?: NSAllDomainsMask];
    for (NSURL *candidate in urls) {
        NSURLRelationship relationship;
        if ([self getRelationship:&relationship ofDirectoryAtURL:candidate toItemAtURL:url error:NULL] &&
            relationship != NSURLRelationshipOther) {
            if (outRelationship) *outRelationship = relationship;
            return YES;
        }
    }
    if (outRelationship)
        *outRelationship = NSURLRelationshipOther;
    return YES;
}

// No cloud containers.
- (NSURL *)URLForUbiquityContainerIdentifier:(NSString *)containerIdentifier {
    return nil;
}

- (id)ubiquityIdentityToken {
    return nil;
}

@end

#pragma mark - NSBundle

static NSURL *directoryURL(NSString *path) {
    return path ? [NSURL fileURLWithPath:path isDirectory:YES] : nil;
}

static NSURL *fileURL(NSString *path) {
    return path ? [NSURL fileURLWithPath:path isDirectory:NO] : nil;
}

@implementation NSBundle (NSURLResources)

+ (NSBundle *)bundleWithURL:(NSURL *)url {
    return [url isFileURL] ? [self bundleWithPath:[url path]] : nil;
}

- (instancetype)initWithURL:(NSURL *)url {
    return [url isFileURL] ? [self initWithPath:[url path]] : nil;
}

- (NSURL *)bundleURL { return directoryURL([self bundlePath]); }
- (NSURL *)resourceURL { return directoryURL([self resourcePath]); }
- (NSURL *)executableURL { return fileURL([self executablePath]); }
- (NSURL *)privateFrameworksURL { return directoryURL([self privateFrameworksPath]); }
- (NSURL *)sharedFrameworksURL { return directoryURL([self sharedFrameworksPath]); }
- (NSURL *)sharedSupportURL { return directoryURL([self sharedSupportPath]); }
- (NSURL *)builtInPlugInsURL { return directoryURL([self builtInPlugInsPath]); }

- (NSURL *)URLForAuxiliaryExecutable:(NSString *)executableName {
    return fileURL([self pathForAuxiliaryExecutable:executableName]);
}

- (NSURL *)appStoreReceiptURL {
    return [NSURL fileURLWithPath:[[self bundlePath] stringByAppendingPathComponent:@"Contents/_MASReceipt/receipt"]
                      isDirectory:NO];
}

- (NSArray *)URLsForResourcesWithExtension:(NSString *)ext subdirectory:(NSString *)subpath {
    NSMutableArray *urls = [NSMutableArray array];
    for (NSString *path in [self pathsForResourcesOfType:ext inDirectory:subpath])
        [urls addObject:[NSURL fileURLWithPath:path]];
    return urls;
}

- (NSArray *)URLsForResourcesWithExtension:(NSString *)ext subdirectory:(NSString *)subpath
                           localization:(NSString *)localizationName {
    NSMutableArray *urls = [NSMutableArray array];
    for (NSString *path in [self pathsForResourcesOfType:ext inDirectory:subpath forLocalization:localizationName])
        [urls addObject:[NSURL fileURLWithPath:path]];
    return urls;
}

+ (NSURL *)URLForResource:(NSString *)name withExtension:(NSString *)ext subdirectory:(NSString *)subpath
          inBundleWithURL:(NSURL *)bundleURL {
    return [[self bundleWithURL:bundleURL] URLForResource:name withExtension:ext subdirectory:subpath];
}

+ (NSArray *)URLsForResourcesWithExtension:(NSString *)ext subdirectory:(NSString *)subpath
                          inBundleWithURL:(NSURL *)bundleURL {
    return [[self bundleWithURL:bundleURL] URLsForResourcesWithExtension:ext subdirectory:subpath];
}

@end
