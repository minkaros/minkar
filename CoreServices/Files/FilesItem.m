/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The file manager's items: a folder's entries read with NSFileManager.
 */

#import "FilesItem.h"
#import "FilesIcons.h"
#import "FilesPreferences.h"

NSLocale *FilesDisplayLocale(void)
{
    return [NSLocale localeWithLocaleIdentifier:[[[NSBundle mainBundle] preferredLocalizations] firstObject]];
}

NSString *FilesSizeString(unsigned long long size)
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

FilesSortKey FilesCurrentSortKey(void)
{
    NSInteger key = [[NSUserDefaults standardUserDefaults] integerForKey:@"FilesSortBy"];
    return key >= FilesSortByName && key <= FilesSortBySize ? (FilesSortKey)key : FilesSortByName;
}

void FilesSetSortKey(FilesSortKey key)
{
    [[NSUserDefaults standardUserDefaults] setInteger:key forKey:@"FilesSortBy"];
}

@implementation FilesItem

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
    _created = [[attributes fileCreationDate] copy];
    if (_isApplication) {
        NSString *display = [[NSBundle bundleWithURL:url] objectForInfoDictionaryKey:@"CFBundleName"];
        if ([display length])
            _name = [display copy];
        _kind = NSLocalizedString(@"Application", nil);
        _icon = [[NSWorkspace sharedWorkspace] iconForFile:[url path]];
    } else if (_isFolder) {
        _kind = NSLocalizedString(@"Folder", nil);
        _icon = FilesFolderIconForURL(url);
        /* Desktop, Documents...: in the user's language (.localized). */
        _name = [[fm displayNameAtPath:[url path]] copy];
    } else {
        NSString *extension = [[url pathExtension] uppercaseString];
        _kind = [extension length] ? [NSString stringWithFormat:NSLocalizedString(@"%@ File", nil), extension] : NSLocalizedString(@"Document", nil);
        _icon = FilesDocumentIcon();
    }
    return self;
}

+ (NSArray<FilesItem *> *)itemsInFolder:(NSURL *)folder error:(NSError **)error
{
    NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[folder path] error:error];
    NSMutableArray *items = [NSMutableArray array];
    BOOL showHidden = FilesShowsHiddenFiles(), foldersFirst = FilesSortsFoldersFirst();

    if (names == nil)
        return nil;
    for (NSString *name in names) {
        if ([name hasPrefix:@"."] && !showHidden)
            continue;
        [items addObject:[[FilesItem alloc] initWithURL:[folder URLByAppendingPathComponent:name]]];
    }
    FilesSortKey key = FilesCurrentSortKey();
    [items sortUsingComparator:^NSComparisonResult(FilesItem *a, FilesItem *b) {
        if (foldersFirst && a.isFolder != b.isFolder)
            return a.isFolder ? NSOrderedAscending : NSOrderedDescending;
        NSComparisonResult r = NSOrderedSame;
        switch (key) {
        case FilesSortByKind:
            r = [a.kind localizedStandardCompare:b.kind];
            break;
        case FilesSortByDateModified:   /* newest first, as the reference */
            r = [b.modified ?: [NSDate distantPast] compare:a.modified ?: [NSDate distantPast]];
            break;
        case FilesSortByDateCreated:
            r = [b.created ?: [NSDate distantPast] compare:a.created ?: [NSDate distantPast]];
            break;
        case FilesSortBySize:           /* largest first */
            r = a.size == b.size ? NSOrderedSame : (a.size > b.size ? NSOrderedAscending : NSOrderedDescending);
            break;
        case FilesSortByName:
            break;
        }
        return r != NSOrderedSame ? r : [a.name localizedStandardCompare:b.name];
    }];
    return items;
}

@end
