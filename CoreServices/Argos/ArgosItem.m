/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Argos's items: a folder's entries read with NSFileManager.
 */

#import "ArgosItem.h"
#import "ArgosIcons.h"

NSString *ArgosSizeString(unsigned long long size)
{
    if (size < 1000)
        return [NSString stringWithFormat:@"%llu bytes", size];
    NSArray *units = @[ @"KB", @"MB", @"GB", @"TB" ];
    double value = size;
    NSUInteger unit = 0;
    value /= 1000;
    while (value >= 1000 && unit + 1 < [units count]) {
        value /= 1000;
        unit++;
    }
    return [NSString stringWithFormat:value < 10 ? @"%.1f %@" : @"%.0f %@", value, units[unit]];
}

@implementation ArgosItem

- (instancetype)initWithURL:(NSURL *)url
{
    if ((self = [super init]) == nil)
        return nil;
    NSFileManager *fm = [NSFileManager defaultManager];
    NSDictionary *attributes = [fm attributesOfItemAtPath:[url path] error:NULL];
    BOOL directory = NO;
    [fm fileExistsAtPath:[url path] isDirectory:&directory];   /* through links too */

    _URL = [url copy];
    _name = [[url lastPathComponent] copy];
    if ([[url path] isEqualToString:@"/"])
        _name = @"Minkar";
    _isApplication = directory && [[[url pathExtension] lowercaseString] isEqualToString:@"app"];
    _isFolder = directory && !_isApplication;
    _size = directory ? 0 : [attributes fileSize];
    _modified = [[attributes fileModificationDate] copy];
    if (_isApplication) {
        NSString *display = [[NSBundle bundleWithURL:url] objectForInfoDictionaryKey:@"CFBundleName"];
        if ([display length])
            _name = [display copy];
        _kind = @"Application";
        _icon = [[NSWorkspace sharedWorkspace] iconForFile:[url path]];
    } else if (_isFolder) {
        _kind = @"Folder";
        _icon = ArgosFolderIcon();
    } else {
        NSString *extension = [[url pathExtension] uppercaseString];
        _kind = [extension length] ? [NSString stringWithFormat:@"%@ File", extension] : @"Document";
        _icon = ArgosDocumentIcon();
    }
    return self;
}

+ (NSArray<ArgosItem *> *)itemsInFolder:(NSURL *)folder error:(NSError **)error
{
    NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[folder path] error:error];
    NSMutableArray *items = [NSMutableArray array];

    if (names == nil)
        return nil;
    for (NSString *name in names) {
        if ([name hasPrefix:@"."])
            continue;
        [items addObject:[[ArgosItem alloc] initWithURL:[folder URLByAppendingPathComponent:name]]];
    }
    [items sortUsingComparator:^NSComparisonResult(ArgosItem *a, ArgosItem *b) {
        if (a.isFolder != b.isFolder)
            return a.isFolder ? NSOrderedAscending : NSOrderedDescending;
        return [a.name localizedStandardCompare:b.name];
    }];
    return items;
}

@end
