/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock's model: persistent tiles from the persistent-apps preference
 * (domain org.minkar.dock), and the running regular apps from NSWorkspace.
 * It changes only on NSWorkspace's notices: nothing polls.
 */

#include <os/log.h>
#import "DockModel.h"
#include <fcntl.h>
#include <notify.h>
#include <SkyLight/SkyLight.h>
#include <dispatch/dispatch.h>
#include <unistd.h>
#include <mach/mach.h>
#include <servers/bootstrap.h>
#include "NotificationCenterTypes.h"
#include "NotificationCenterUser.h"     /* MIG, generated */

NSNotificationName const DockModelDidChangeNotification = @"DockModelDidChangeNotification";

static NSString *const kPersistentApps = @"persistent-apps";
static NSString *const kRecentApps = @"recent-apps";
static const NSUInteger kMaxRecentApps = 3;

/* Atlas's icons (SystemLibrary/Icons). */
/* The Trash's icons, the system's or the icon theme's. */
extern NSString *_NSSystemIconPath(NSString *relative);
#define kTrashEmptyIconPath _NSSystemIconPath(@"system/user-trash.png")
#define kTrashFullIconPath _NSSystemIconPath(@"system/user-trash-full.png")

@interface DockTile ()
@property (nonatomic, strong) NSImage *icon;
- (instancetype)initTrash;
- (instancetype)initMinimizedWindow:(uint32_t)windowID owner:(pid_t)pid image:(NSImage *)image;
@end

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

- (instancetype)initTrash
{
    if ((self = [super init]) != nil) {
        _kind = DockTileKindTrash;
        _URL = [DockTrashURL() copy];
        _label = [NSLocalizedString(@"Trash", nil) copy];
        _icon = [[NSImage alloc] initWithContentsOfFile:kTrashEmptyIconPath];
        if (_icon == nil)
            _icon = [[NSWorkspace sharedWorkspace] iconForFile:[_URL path]];
    }
    return self;
}

- (instancetype)initMinimizedWindow:(uint32_t)windowID owner:(pid_t)pid image:(NSImage *)image
{
    if ((self = [super init]) != nil) {
        NSRunningApplication *app = [NSRunningApplication runningApplicationWithProcessIdentifier:pid];

        _kind = DockTileKindMinimizedWindow;
        _windowID = windowID;
        _ownerPID = pid;
        _URL = [([app bundleURL] ?: [NSURL fileURLWithPath:@"/"]) copy];
        _label = [([app localizedName] ?: @"") copy];
        _icon = image ?: [app icon];
    }
    return self;
}

- (BOOL)matchesApplication:(NSRunningApplication *)app
{
    if (_kind == DockTileKindMinimizedWindow)
        return NO;
    if (_kind == DockTileKindTrash)
        return NO;
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

NSURL *DockTrashURL(void)
{
    return [NSURL fileURLWithPath:[NSHomeDirectory() stringByAppendingPathComponent:@".Trash"] isDirectory:YES];
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

/* The utilities the Dock keeps too (the maintainer's choice, 23 Nov 2026):
   Terminal, System Monitor and Logs. */
static NSArray<NSString *> *utilityApps(void)
{
    return @[ @"/System/Applications/Utilities/Terminal.app",
              @"/System/Applications/Utilities/SystemMonitor.app",
              @"/System/Applications/Utilities/Logs.app" ];
}

/* With no preference yet: the file manager, the installed applications,
   then the utilities. */
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
    for (NSString *path in utilityApps())
        if ([fm fileExistsAtPath:path])
            [urls addObject:[NSURL fileURLWithPath:path]];
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
    NSDictionary<NSString *, NSString *> *_badges;   /* process identifier to label */
    NSArray<DockTile *> *_persistent;
    NSArray<DockTile *> *_tiles;
    NSMutableArray<DockTile *> *_minimized;
    NSMutableArray<NSURL *> *_recent;           /* newest first */
    NSMutableArray<DockTile *> *_recentTiles;
    /* minimize-to-application: the windows each app holds (pid -> IDs). */
    NSMutableDictionary<NSNumber *, NSMutableArray<NSNumber *> *> *_minimizedInApps;
    dispatch_source_t _trashWatch;
}

@synthesize autohide = _autohide;

/* The preference, or the front app's presentation options asking it. */
- (BOOL)autohide
{
    return _autohide || _presentationAutohide;
}

- (void)setPresentationAutohide:(BOOL)value
{
    if (value == _presentationAutohide)
        return;
    _presentationAutohide = value;
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
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
    /* Absent, these take the reference platform's defaults. */
    _showIndicators = [defaults objectForKey:@"show-process-indicators"] ? [defaults boolForKey:@"show-process-indicators"] : YES;
    _launchAnimation = [defaults objectForKey:@"launchanim"] ? [defaults boolForKey:@"launchanim"] : YES;
    _minimizeToApplication = [defaults boolForKey:@"minimize-to-application"];
    _minimizeEffect = [[defaults stringForKey:@"mineffect"] isEqualToString:@"scale"] ? DockMinimizeEffectScale
                                                                                     : DockMinimizeEffectGenie;
    _showRecents = [defaults objectForKey:@"show-recents"] ? [defaults boolForKey:@"show-recents"] : YES;
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

/* The apps' badges (Atlas.md, 5.6, N2): notificationd keeps them and
   posts kNCBadgesChanged; they are asked for again then. */
- (void)watchBadges
{
    int fd = -1, token;

    [self badgesChanged];
    if (notify_register_file_descriptor(kNCBadgesChanged, &fd, 0, &token) != NOTIFY_STATUS_OK)
        return;
    [NSThread detachNewThreadWithBlock:^{
        int value;
        while (read(fd, &value, sizeof(value)) == sizeof(value))
            [self performSelectorOnMainThread:@selector(badgesChanged) withObject:nil waitUntilDone:NO];
    }];
    int sfd = -1, stoken;
    if (notify_register_file_descriptor(kNCSettingsChanged, &sfd, 0, &stoken) != NOTIFY_STATUS_OK)
        return;
    [NSThread detachNewThreadWithBlock:^{
        int value;
        while (read(sfd, &value, sizeof(value)) == sizeof(value))
            [self performSelectorOnMainThread:@selector(notificationSettingsChanged) withObject:nil waitUntilDone:NO];
    }];
}

/* The Notifications pane changed an app's Badge app icon. */
- (void)notificationSettingsChanged
{
    CFPreferencesAppSynchronize(CFSTR(kNCSettingsDomain));
    NSUInteger shown = 0;
    for (DockTile *tile in self.tiles)
        if ([self badgeForTile:tile] != nil)
            shown++;
    fprintf(stderr, "Dock: %lu badges shown\n", (unsigned long)shown);
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
}

- (void)badgesChanged
{
    mach_port_t server = MACH_PORT_NULL;
    char bytes[kNCMaxPayload];
    mach_msg_type_number_t length = 0;
    int32_t result = kNCNoService;
    NSDictionary *badges = nil;

    if (bootstrap_look_up(bootstrap_port, kNCServiceName, &server) == KERN_SUCCESS &&
        ncc_copy_badges(server, bytes, &length, &result) == KERN_SUCCESS && result == kNCSuccess) {
        id plist = [NSPropertyListSerialization propertyListWithData:[NSData dataWithBytes:bytes length:length]
                                                             options:0 format:NULL error:NULL];
        if ([plist isKindOfClass:[NSDictionary class]])
            badges = plist;
    }
    if (server != MACH_PORT_NULL)
        mach_port_deallocate(mach_task_self(), server);
    if (badges == nil || [badges isEqual:_badges])
        return;
    _badges = [badges copy];
    fprintf(stderr, "Dock: %lu badges\n", (unsigned long)_badges.count);
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
}

/* Badge app icon, an app's setting in the Notifications pane: on when
   absent. */
static BOOL badgeAllowed(NSRunningApplication *app)
{
    NSString *name = app.bundleIdentifier.length > 0 ? app.bundleIdentifier : app.localizedName;
    if (name.length == 0)
        return YES;
    NSString *key = [@kNCSettingAppPrefix stringByAppendingString:name];
    CFPropertyListRef value = CFPreferencesCopyAppValue((__bridge CFStringRef)key, CFSTR(kNCSettingsDomain));
    BOOL allowed = YES;

    if (value != NULL && CFGetTypeID(value) == CFDictionaryGetTypeID()) {
        id badge = ((__bridge NSDictionary *)value)[@kNCAppBadge];
        id permitted = ((__bridge NSDictionary *)value)[@kNCAppAllowed];
        allowed = (badge == nil || [badge boolValue]) && (permitted == nil || [permitted boolValue]);
    }
    if (value != NULL)
        CFRelease(value);
    return allowed;
}

- (NSString *)badgeForTile:(DockTile *)tile
{
    if (tile.application == nil || !badgeAllowed(tile.application))
        return nil;
    NSString *label = _badges[[NSString stringWithFormat:@"%d", tile.application.processIdentifier]];
    return label.length > 0 ? label : nil;
}

- (void)preferencesChanged
{
    [self reloadPreferences];
    [self rebuild];         /* show-recents adds or takes tiles */
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

- (DockTile *)tileForApplicationURL:(NSURL *)url
{
    return [[DockTile alloc] initWithKind:DockTileKindRunningApp URL:url];
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
    [self watchBadges];

    NSUserDefaults *defaults = [NSUserDefaults standardUserDefaults];
    NSArray *preference = [defaults arrayForKey:kPersistentApps];
    NSArray<NSURL *> *urls;

    if (preference == nil) {
        NSMutableArray *entries = [NSMutableArray array];
        urls = defaultPersistentApps();
        for (NSURL *url in urls)
            [entries addObject:DockPersistentAppEntry(url)];
        [defaults setObject:entries forKey:kPersistentApps];
        [defaults setBool:YES forKey:@"minkar-utilities-added"];
        [defaults synchronize];
    } else {
        urls = DockPersistentAppURLs(preference);
        /* A Dock kept from before the utilities were added gets them once;
           one taken out afterwards stays out. */
        if (![defaults boolForKey:@"minkar-utilities-added"]) {
            NSMutableArray *entries = [preference mutableCopy];
            NSMutableArray<NSURL *> *all = [urls mutableCopy];
            for (NSString *path in utilityApps()) {
                NSURL *url = [NSURL fileURLWithPath:path];
                BOOL there = NO;
                for (NSURL *u in urls)
                    there |= [[[u path] stringByStandardizingPath] isEqualToString:path];
                if (!there && [[NSFileManager defaultManager] fileExistsAtPath:path]) {
                    [entries addObject:DockPersistentAppEntry(url)];
                    [all addObject:url];
                }
            }
            [defaults setObject:entries forKey:kPersistentApps];
            [defaults setBool:YES forKey:@"minkar-utilities-added"];
            [defaults synchronize];
            urls = all;
        }
    }

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
    /* An icon theme chosen: the Trash's icon from it. */
    [[NSNotificationCenter defaultCenter] addObserver:self selector:@selector(themesChanged:)
                                                 name:@"_NSSystemThemesChangedNotification" object:nil];
    _recent = [NSMutableArray array];
    for (NSURL *url in DockPersistentAppURLs([defaults arrayForKey:kRecentApps] ?: @[]))
        if ([[NSFileManager defaultManager] fileExistsAtPath:[url path]] && [_recent count] < kMaxRecentApps)
            [_recent addObject:url];
    _recentTiles = [NSMutableArray array];
    _minimizedInApps = [NSMutableDictionary dictionary];
    _trashTile = [[DockTile alloc] initTrash];
    [self watchTrash];
    [self rebuild];
    return self;
}

/* -- The Trash -- */

- (void)updateTrashIcon
{
    NSArray *names = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:[DockTrashURL() path] error:NULL];
    BOOL full = NO;

    for (NSString *name in names)
        if (![name isEqualToString:@".DS_Store"]) {
            full = YES;
            break;
        }
    if (full == _trashIsFull && _trashTile.icon != nil)
        return;
    _trashIsFull = full;
    NSImage *icon = [[NSImage alloc] initWithContentsOfFile:full ? kTrashFullIconPath : kTrashEmptyIconPath];
    if (icon != nil)
        _trashTile.icon = icon;
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
}

/* The folder's changes, from the kernel (no polling). */
- (void)themesChanged:(NSNotification *)note
{
    _trashIsFull = !_trashIsFull;   /* so that the icon is loaded again */
    [self updateTrashIcon];
}

- (void)watchTrash
{
    NSString *path = [DockTrashURL() path];

    [[NSFileManager defaultManager] createDirectoryAtPath:path withIntermediateDirectories:YES
                                               attributes:@{ NSFilePosixPermissions: @0700 } error:NULL];
    int fd = open([path fileSystemRepresentation], O_EVTONLY);
    if (fd < 0)
        return;
    _trashWatch = dispatch_source_create(DISPATCH_SOURCE_TYPE_VNODE, (uintptr_t)fd,
        DISPATCH_VNODE_WRITE | DISPATCH_VNODE_DELETE | DISPATCH_VNODE_RENAME, dispatch_get_main_queue());
    __weak DockModel *weakSelf = self;
    dispatch_source_set_event_handler(_trashWatch, ^{
        [weakSelf updateTrashIcon];
    });
    dispatch_source_set_cancel_handler(_trashWatch, ^{
        close(fd);
    });
    dispatch_resume(_trashWatch);
    [self updateTrashIcon];
}

- (void)emptyTrash
{
    NSFileManager *fm = [NSFileManager defaultManager];
    NSString *path = [DockTrashURL() path];

    for (NSString *name in [fm contentsOfDirectoryAtPath:path error:NULL])
        [fm removeItemAtPath:[path stringByAppendingPathComponent:name] error:NULL];
    [self updateTrashIcon];
}

- (void)dealloc
{
    if (_trashWatch != nil)
        dispatch_source_cancel(_trashWatch);
    [[[NSWorkspace sharedWorkspace] notificationCenter] removeObserver:self];
}

- (NSArray<DockTile *> *)tiles
{
    return _tiles;
}

- (void)saveRecent
{
    NSMutableArray *entries = [NSMutableArray array];

    for (NSURL *url in _recent)
        [entries addObject:DockPersistentAppEntry(url)];
    [[NSUserDefaults standardUserDefaults] setObject:entries forKey:kRecentApps];
    [[NSUserDefaults standardUserDefaults] synchronize];
}

/* The tiles again: the persistent ones with their running instances,
   then the regular apps that run and are not kept, then the recent ones. */
- (void)rebuild
{
    NSMutableArray<DockTile *> *tiles = [NSMutableArray arrayWithArray:_persistent];
    NSMutableArray<DockTile *> *running = [NSMutableArray array];
    NSMutableArray<NSURL *> *quit = [NSMutableArray array];

    /* What ran before and is not kept: those that quit become recent. */
    for (DockTile *tile in _tiles)
        if (tile.application != nil && tile.kind == DockTileKindRunningApp && tile.URL != nil)
            [quit addObject:tile.URL];

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
            /* The tile it had, with what it drew and its bounce (D3). */
            for (DockTile *old in _tiles)
                if (old.kind == DockTileKindRunningApp && [old.URL isEqual:url] && ![running containsObject:old])
                    owner = old;
            if (owner == nil)
                owner = [[DockTile alloc] initWithKind:DockTileKindRunningApp URL:url];
            [running addObject:owner];
        }
        owner.application = app;
    }
    /* A kept app that quit: its icon again, still. */
    for (DockTile *tile in _persistent)
        if (tile.application == nil) {
            tile.contentImage = nil;
            tile.bounceStart = 0;
        }
    /* An app not kept that quit: the most recent of the recent ones. */
    for (DockTile *tile in running)
        [quit removeObject:tile.URL];
    BOOL recentChanged = NO;
    for (NSURL *url in quit) {
        [_recent removeObject:url];
        [_recent insertObject:url atIndex:0];
        recentChanged = YES;
    }
    for (NSURL *url in [_recent copy]) {
        BOOL inDock = NO;
        for (DockTile *tile in tiles)
            inDock |= [tile.URL isEqual:url];
        for (DockTile *tile in running)
            inDock |= [tile.URL isEqual:url];
        if (inDock) {
            [_recent removeObject:url];
            recentChanged = YES;
        }
    }
    while ([_recent count] > kMaxRecentApps) {
        [_recent removeLastObject];
        recentChanged = YES;
    }
    if (recentChanged)
        [self saveRecent];
    NSMutableArray<DockTile *> *recentTiles = [NSMutableArray array];
    if (_showRecents)
        for (NSURL *url in _recent) {
            DockTile *recent = nil;
            for (DockTile *old in _recentTiles)
                if ([old.URL isEqual:url])
                    recent = old;
            [recentTiles addObject:recent ?: [[DockTile alloc] initWithKind:DockTileKindRecentApp URL:url]];
        }
    _recentTiles = recentTiles;

    _firstRunningIndex = [tiles count];
    [tiles addObjectsFromArray:running];
    [tiles addObjectsFromArray:recentTiles];
    _minimizedIndex = [tiles count];
    if (_minimized != nil)
        [tiles addObjectsFromArray:_minimized];
    _trashIndex = [tiles count];
    if (_trashTile != nil)
        [tiles addObject:_trashTile];
    _tiles = [tiles copy];
}

- (void)applicationsChanged:(NSNotification *)note
{
    /* An app that quit takes its minimized windows with it. */
    if ([note.name isEqualToString:NSWorkspaceDidTerminateApplicationNotification]) {
        pid_t gone = [note.userInfo[NSWorkspaceApplicationKey] processIdentifier];
        if (gone > 0) {
            NSIndexSet *theirs = [_minimized indexesOfObjectsPassingTest:^BOOL(DockTile *tile, NSUInteger i, BOOL *stop) {
                return tile.ownerPID == gone;
            }];
            [_minimized removeObjectsAtIndexes:theirs];
            [_minimizedInApps removeObjectForKey:@(gone)];
        }
    }
    [self rebuild];
    [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
}

- (NSUInteger)addMinimizedWindow:(uint32_t)windowID owner:(pid_t)pid image:(NSImage *)image
{
    /* Into its app's tile: no tile of its own. */
    if (_minimizeToApplication) {
        NSMutableArray *windows = _minimizedInApps[@(pid)] ?: [NSMutableArray array];
        [windows removeObject:@(windowID)];
        [windows addObject:@(windowID)];
        _minimizedInApps[@(pid)] = windows;
        for (NSUInteger i = 0; i < [_tiles count]; i++)
            if (_tiles[i].kind != DockTileKindMinimizedWindow && _tiles[i].application != nil &&
                [_tiles[i].application processIdentifier] == pid)
                return i;
        return _trashIndex - 1;
    }
    if (_minimized == nil)
        _minimized = [NSMutableArray array];
    [self removeMinimizedWindow:windowID quietly:YES];
    [_minimized addObject:[[DockTile alloc] initMinimizedWindow:windowID owner:pid image:image]];
    [self applicationsChanged:nil];
    return _trashIndex - 1;
}

- (DockTile *)tileForMinimizedWindow:(uint32_t)windowID
{
    for (DockTile *tile in _minimized)
        if (tile.windowID == windowID)
            return tile;
    return nil;
}

- (void)removeMinimizedWindow:(uint32_t)windowID quietly:(BOOL)quietly
{
    DockTile *tile = [self tileForMinimizedWindow:windowID];

    for (NSNumber *pid in [_minimizedInApps allKeys])
        [_minimizedInApps[pid] removeObject:@(windowID)];

    if (tile == nil)
        return;
    [_minimized removeObject:tile];
    if (!quietly)
        [self applicationsChanged:nil];
}

- (void)removeMinimizedWindow:(uint32_t)windowID
{
    [self removeMinimizedWindow:windowID quietly:NO];
}

- (BOOL)openTile:(DockTile *)tile
{
    /* The server restores it; its owner hears it and brings it back. */
    if (tile.kind == DockTileKindMinimizedWindow)
        return SLSSetWindowMinimized(tile.windowID, 0) == kSLSErrorSuccess;
    if (tile.kind == DockTileKindTrash)
        return [[NSWorkspace sharedWorkspace] openFile:[tile.URL path]];
    /* An app the Dock starts: its tile bounces until the app checks in
       with the session, which it does as it finishes launching. */
    if (_launchAnimation && (tile.application == nil || [tile.application isTerminated])) {
        tile.bounceStart = CFAbsoluteTimeGetCurrent();
        tile.bounceLaunch = YES;
        tile.bounceUntil = 0;
        os_log_info(OS_LOG_DEFAULT, "launch bounce %{public}s", [tile.label UTF8String]);
        [[NSNotificationCenter defaultCenter] postNotificationName:DockModelDidChangeNotification object:self];
    }
    if (tile.application != nil && ![tile.application isTerminated]) {
        /* Windows minimized into it: the last one back. */
        NSMutableArray *windows = _minimizedInApps[@([tile.application processIdentifier])];
        if ([windows count] > 0) {
            uint32_t windowID = [[windows lastObject] unsignedIntValue];
            [windows removeLastObject];
            SLSSetWindowMinimized(windowID, 0);
        }
        return [tile.application activateWithOptions:NSApplicationActivateIgnoringOtherApps];
    }
    return [[NSWorkspace sharedWorkspace] openFile:[tile.URL path]];
}

@end
