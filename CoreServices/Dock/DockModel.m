/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock's model: persistent tiles from the persistent-apps preference
 * (domain org.minkar.dock), and the running regular apps from NSWorkspace.
 * It changes only on NSWorkspace's notices: nothing polls.
 */

#import "DockModel.h"
#include <notify.h>
#include <unistd.h>

NSNotificationName const DockModelDidChangeNotification = @"DockModelDidChangeNotification";

static NSString *const kPersistentApps = @"persistent-apps";

@implementation DockTile

- (instancetype)initWithKind:(DockTileKind)kind URL:(NSURL *)url
{
    if ((self = [super init]) != nil) {
        NSBundle *bundle = [NSBundle bundleWithURL:url];
        NSString *name = [bundle objectForInfoDictionaryKey:@"CFBundleDisplayName"];

        if (name == nil)
            name = [bundle objectForInfoDictionaryKey:@"CFBundleName"];
        if (name == nil)
            name = [[url lastPathComponent] stringByDeletingPathExtension];
        _kind = kind;
        _URL = [url copy];
        _label = [name copy];
        _bundleIdentifier = [[bundle bundleIdentifier] copy];
        _icon = [[NSWorkspace sharedWorkspace] iconForFile:[url path]];
    }
    return self;
}

- (BOOL)matchesApplication:(NSRunningApplication *)app
{
    if (_bundleIdentifier != nil && [app bundleIdentifier] != nil)
        return [_bundleIdentifier isEqualToString:[app bundleIdentifier]];
    return [[[app bundleURL] path] isEqualToString:[_URL path]];
}

@end

NSArray<NSURL *> *DockPersistentAppURLs(NSArray *preference)
{
    NSMutableArray<NSURL *> *urls = [NSMutableArray array];

    for (id entry in preference) {
        if (![entry isKindOfClass:[NSDictionary class]])
            continue;
        NSString *string = entry[@"tile-data"][@"file-data"][@"_CFURLString"];
        if (![string isKindOfClass:[NSString class]])
            continue;
        NSURL *url = [string hasPrefix:@"/"] ? [NSURL fileURLWithPath:string]
                                             : [NSURL URLWithString:string];
        if (url != nil && [url isFileURL])
            [urls addObject:url];
    }
    return urls;
}

NSDictionary *DockPersistentAppEntry(NSURL *url)
{
    return @{
        @"tile-type": @"file-tile",
        @"tile-data": @{
            @"file-label": [[url lastPathComponent] stringByDeletingPathExtension],
            @"file-data": @{
                @"_CFURLString": [url absoluteString],
                @"_CFURLStringType": @15,
            },
        },
    };
}

/* With no preference yet: the file manager, then the installed applications. */
static NSArray<NSURL *> *defaultPersistentApps(void)
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSMutableArray<NSURL *> *urls = [NSMutableArray array];
    NSString *files = @"/System/Library/CoreServices/Files.app";

    if ([fm fileExistsAtPath:files])
        [urls addObject:[NSURL fileURLWithPath:files]];
    for (NSString *dir in @[ @"/System/Applications", @"/Applications" ]) {
        NSArray *names = [[fm contentsOfDirectoryAtPath:dir error:NULL]
            sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)];
        for (NSString *name in names)
            if ([[name pathExtension] isEqualToString:@"app"])
                [urls addObject:[NSURL fileURLWithPath:[dir stringByAppendingPathComponent:name]]];
    }
    return urls;
}

static CGFloat sizePreference(NSUserDefaults *defaults, NSString *key, CGFloat fallback)
{
    id value = [defaults objectForKey:key];

    if (![value respondsToSelector:@selector(doubleValue)])
        return fallback;
    return MIN(MAX([value doubleValue], 16), 128);
}

@implementation DockModel {
    NSArray<DockTile *> *_persistent;
    NSArray<DockTile *> *_tiles;
}

- (void)reloadPreferences
{
    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSString *orientation;

    [defaults synchronize];
    _tileSize = sizePreference(defaults, @"tilesize", 48);
    _largeSize = sizePreference(defaults, @"largesize", 64);
    _magnification = [defaults boolForKey:@"magnification"];
    _autohide = [defaults boolForKey:@"autohide"];
    orientation = [defaults stringForKey:@"orientation"];
    if ([orientation isEqualToString:@"left"])
        _orientation = DockOrientationLeft;
    else if ([orientation isEqualToString:@"right"])
        _orientation = DockOrientationRight;
    else
        _orientation = DockOrientationBottom;
}

/* notify(3)'s name, read on a thread of its own; the main thread reloads. */
- (void)watchPreferences
{
    int fd = -1, token;

    if (notify_register_file_descriptor(kDockPreferencesChangedNotifyName, &fd, 0, &token) != NOTIFY_STATUS_OK)
        return;
    [NSThread detachNewThreadWithBlock:^{
        int value;
        while (read(fd, &value, sizeof(value)) == sizeof(value))
            [self performSelectorOnMainThread:@selector(preferencesChanged) withObject:nil waitUntilDone:NO];
    }];
}

- (void)preferencesChanged
{
    [self reloadPreferences];
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
}

- (void)savePersistent
{
    NSMutableArray *entries = [NSMutableArray array];

    for (DockTile *tile in _persistent)
        [entries addObject:DockPersistentAppEntry(tile.URL)];
    [[NSUserDefaults standardUserDefaults] setObject:entries forKey:kPersistentApps];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

- (BOOL)isKept:(DockTile *)tile
{
    return [_persistent indexOfObjectIdenticalTo:tile] != NSNotFound;
}

- (void)keepTile:(DockTile *)tile atIndex:(NSUInteger)index
{
    NSMutableArray *persistent = [_persistent mutableCopy];
    NSUInteger old = [persistent indexOfObjectIdenticalTo:tile];
    DockTile *kept = tile;

    if (old != NSNotFound) {
        [persistent removeObjectAtIndex:old];
        if (index > old)
            index--;
    } else {
        kept = [[DockTile alloc] initWithKind:DockTileKindPersistentApp URL:tile.URL];
    }
    [persistent insertObject:kept atIndex:MIN(index, [persistent count])];
    _persistent = [persistent copy];
    [self savePersistent];
    [self applicationsChanged:nil];
}

- (void)removeTile:(DockTile *)tile
{
    NSMutableArray *persistent = [_persistent mutableCopy];

    [persistent removeObjectIdenticalTo:tile];
    _persistent = [persistent copy];
    [self savePersistent];
    [self applicationsChanged:nil];
}

- (instancetype)init
{
    if ((self = [super init]) == nil)
        return nil;
    [self reloadPreferences];
    [self watchPreferences];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *preference = [defaults arrayForKey:kPersistentApps];
    NSArray<NSURL *> *urls;

    if (preference == nil) {
        NSMutableArray *entries = [NSMutableArray array];
        urls = defaultPersistentApps();
        for (NSURL *url in urls)
            [entries addObject:DockPersistentAppEntry(url)];
        [defaults setObject:entries forKey:kPersistentApps];
        [defaults synchronize];
    } else
        urls = DockPersistentAppURLs(preference);

    NSMutableArray *persistent = [NSMutableArray array];
    for (NSURL *url in urls)
        if ([[NSFileManager defaultManager] fileExistsAtPath:[url path]])
            [persistent addObject:[[DockTile alloc] initWithKind:DockTileKindPersistentApp URL:url]];
    _persistent = [persistent copy];

    NSNotificationCenter *center = [[NSWorkspace sharedWorkspace] notificationCenter];
    for (NSNotificationName name in @[ NSWorkspaceDidLaunchApplicationNotification,
                                       NSWorkspaceDidTerminateApplicationNotification,
                                       NSWorkspaceDidActivateApplicationNotification ])
        [center addObserver:self selector:@selector(applicationsChanged:) name:name object:nil];
    [self rebuild];
    return self;
}

- (void)dealloc
{
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

- (NSArray<DockTile *> *)tiles
{
    return _tiles;
}

/* The tiles again: the persistent ones with their running instances,
   then the regular apps that run and are not kept. */
- (void)rebuild
{
    NSMutableArray<DockTile *> *tiles = [NSMutableArray arrayWithArray:_persistent];
    NSMutableArray<DockTile *> *running = [NSMutableArray array];

    for (DockTile *tile in _persistent)
        tile.application = nil;
    for (NSRunningApplication *app in [[NSWorkspace sharedWorkspace] runningApplications]) {
        if ([app activationPolicy] != NSApplicationActivationPolicyRegular || [app isTerminated])
            continue;
        DockTile *owner = nil;
        for (DockTile *tile in tiles)
            if ([tile matchesApplication:app]) {
                owner = tile;
                break;
            }
        if (owner == nil) {
            NSURL *url = [app bundleURL] ? [app bundleURL] : [app executableURL];
            if (url == nil)
                continue;
            owner = [[DockTile alloc] initWithKind:DockTileKindRunningApp URL:url];
            [running addObject:owner];
        }
        owner.application = app;
    }
    _firstRunningIndex = [tiles count];
    [tiles addObjectsFromArray:running];
    _tiles = [tiles copy];
}

- (void)applicationsChanged:(NSNotification *)note
{
    [self rebuild];
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
}

- (BOOL)openTile:(DockTile *)tile
{
    if (tile.application != nil && ![tile.application isTerminated])
        return [tile.application activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    return [[NSWorkspace sharedWorkspace] openFile:[tile.URL path]];
}

@end
