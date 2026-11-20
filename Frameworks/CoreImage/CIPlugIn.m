/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreImage/CIPlugIn.h>
#import <CoreImage/CIPlugInInterface.h>
#import <CoreImage/CIFilter.h>
#import <CoreImage/CIFilterConstructor.h>

/* The Info.plist keys of an image unit. */
static NSString *const CIPlugInFilterListKey = @"CIPlugInFilterList";
static NSString *const CIFilterClassKey = @"CIFilterClass";
static NSString *const CIKernelFileKey = @"CIKernelFile";

/* A unit's filter made by its class. */
@interface _CIPlugInFilterConstructor : NSObject <CIFilterConstructor>
@property (nonatomic) Class filterClass;
@end

@implementation _CIPlugInFilterConstructor
- (CIFilter *)filterWithName:(NSString *)name {
    return [[self.filterClass alloc] init];
}
@end

@implementation CIPlugIn

/* The units loaded (their paths), each once. */
static NSMutableSet<NSString *> *loadedUnits(void) {
    static NSMutableSet *units;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ units = [NSMutableSet set]; });
    return units;
}

static NSArray<NSURL *> *unitFolders(void) {
    NSMutableArray<NSURL *> *folders = [NSMutableArray array];
    for (NSNumber *domain in @[ @(NSUserDomainMask), @(NSLocalDomainMask), @(NSSystemDomainMask) ]) {
        NSURL *library = [[NSFileManager defaultManager] URLsForDirectory:NSLibraryDirectory inDomains:domain.unsignedIntegerValue].firstObject;
        if (library != nil)
            [folders addObject:[library URLByAppendingPathComponent:@"Graphics/Image Units" isDirectory:YES]];
    }
    return folders;
}

static NSArray<NSURL *> *unitsInFolders(void) {
    NSMutableArray<NSURL *> *units = [NSMutableArray array];
    for (NSURL *folder in unitFolders()) {
        NSArray<NSURL *> *contents = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:folder includingPropertiesForKeys:nil
            options:NSDirectoryEnumerationSkipsHiddenFiles error:NULL];
        for (NSURL *url in [contents sortedArrayUsingComparator:^NSComparisonResult(NSURL *a, NSURL *b) { return [a.path compare:b.path]; }])
            if ([url.pathExtension isEqualToString:@"plugin"])
                [units addObject:url];
    }
    return units;
}

/* Loads a unit: its code when allowed (its principal class told load:),
   then registers the filters it lists that it implements. A filter made
   only of kernels (CIKernelFile) is not registered: Minkar does not run
   Core Image kernels yet. */
static void loadUnit(NSURL *url, BOOL allowExecutableCode) {
    NSString *path = url.URLByStandardizingPath.path;
    if (path == nil)
        return;
    @synchronized(loadedUnits()) {
        if ([loadedUnits() containsObject:path])
            return;
        [loadedUnits() addObject:path];
    }

    NSBundle *bundle = [NSBundle bundleWithURL:url];
    NSDictionary<NSString *, NSDictionary *> *filters = [bundle objectForInfoDictionaryKey:CIPlugInFilterListKey];
    if (bundle == nil || ![filters isKindOfClass:[NSDictionary class]])
        return;

    BOOL executable = bundle.executableURL != nil;
    if (executable) {
        if (!allowExecutableCode || ![bundle load])
            return;
        Class principal = bundle.principalClass;
        if (principal != Nil && [principal instancesRespondToSelector:@selector(load:)]) {
            id<CIPlugInRegistration> registration = [[principal alloc] init];
            if (![registration load:NULL])
                return;
        }
    }

    [filters enumerateKeysAndObjectsUsingBlock:^(NSString *name, NSDictionary *attributes, BOOL *stop) {
        if (![attributes isKindOfClass:[NSDictionary class]])
            return;
        Class filterClass = executable ? [bundle classNamed:attributes[CIFilterClassKey] ?: name] : Nil;
        if (filterClass == Nil || ![filterClass isSubclassOfClass:[CIFilter class]])
            return;
        _CIPlugInFilterConstructor *constructor = [_CIPlugInFilterConstructor new];
        constructor.filterClass = filterClass;
        [CIFilter registerFilterName:name constructor:constructor classAttributes:attributes];
    }];
}

+ (void)loadAllPlugIns {
    for (NSURL *url in unitsInFolders())
        loadUnit(url, YES);
}

+ (void)loadNonExecutablePlugIns {
    for (NSURL *url in unitsInFolders())
        loadUnit(url, NO);
}

+ (void)loadNonExecutablePlugIn:(NSURL *)url {
    loadUnit(url, NO);
}

+ (void)loadPlugIn:(NSURL *)url allowExecutableCode:(BOOL)allowExecutableCode {
    loadUnit(url, allowExecutableCode);
}

+ (void)loadPlugIn:(NSURL *)url allowNonExecutable:(BOOL)allowNonExecutable {
    loadUnit(url, YES);
}

@end
