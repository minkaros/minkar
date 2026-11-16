/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock's model (Atlas.md, section 4.4): its tiles, as plain data.
 * Views only display it.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, DockTileKind) {
    DockTileKindPersistentApp,   /* kept in the Dock (persistent-apps) */
    DockTileKindRunningApp,      /* running, not kept */
    DockTileKindMinimizedWindow, /* a window minimized into the Dock */
    DockTileKindTrash,           /* the Trash, always last */
};

@interface DockTile : NSObject
@property (nonatomic, readonly) DockTileKind kind;
@property (nonatomic, readonly, copy) NSURL *URL;
@property (nonatomic, readonly, copy) NSString *label;
@property (nonatomic, readonly, nullable, copy) NSString *bundleIdentifier;
@property (nonatomic, readonly) NSImage *icon;
/* The running instance, or nil. */
@property (nonatomic, nullable, strong) NSRunningApplication *application;
/* What the app drew for its tile (NSDockTile's content view), shown in
   place of its icon; nil for the icon (D3). */
@property (nonatomic, nullable, strong) NSImage *contentImage;
/* Bouncing for attention since then (0: not), and until the app is
   active (critical) or for two bounces (informational). */
@property (nonatomic) CFAbsoluteTime bounceStart;
@property (nonatomic) BOOL bounceCritical;
/* A minimized window's tile: the server's window, its owner's pid. */
@property (nonatomic, readonly) uint32_t windowID;
@property (nonatomic, readonly) pid_t ownerPID;
@end

typedef NS_ENUM(NSInteger, DockOrientation) {
    DockOrientationBottom,
    DockOrientationLeft,
    DockOrientationRight,
};

/* Posted on a model change: its tiles, or its preferences. */
extern NSNotificationName const DockModelDidChangeNotification;

/* notify(3) name: the domain's preferences changed, read them again
   (System Preferences' Dock pane posts it). */
#define kDockPreferencesChangedNotifyName "org.minkar.dock.prefs"

@interface DockModel : NSObject
/* The domain's preferences, with the reference platform's keys. */
@property (nonatomic, readonly) CGFloat tileSize;        /* tilesize, 16-128 */
@property (nonatomic, readonly) BOOL magnification;      /* magnification */
@property (nonatomic, readonly) CGFloat largeSize;       /* largesize, 16-128 */
@property (nonatomic, readonly) DockOrientation orientation; /* orientation */
@property (nonatomic, readonly) BOOL autohide;           /* autohide */

/* Persistent tiles first, then the running apps that are not kept. */
@property (nonatomic, readonly, copy) NSArray<DockTile *> *tiles;
/* Index of the first running-only tile (the separator goes before it);
   the Trash's index when none runs. */
@property (nonatomic, readonly) NSUInteger firstRunningIndex;
/* The Trash: the last tile, after a separator of its own. */
@property (nonatomic, readonly) NSUInteger trashIndex;
/* The first minimized window's tile, or trashIndex when there is none. */
@property (nonatomic, readonly) NSUInteger minimizedIndex;
@property (nonatomic, readonly) DockTile *trashTile;
/* Whether the Trash holds anything (its icon shows it). */
@property (nonatomic, readonly) BOOL trashIsFull;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
/* Launches the tile's app, or activates it when it runs. */
- (BOOL)openTile:(DockTile *)tile;
/* Kept in the Dock or not (Keep in Dock, Remove from Dock). */
- (BOOL)isKept:(DockTile *)tile;
- (void)keepTile:(DockTile *)tile atIndex:(NSUInteger)index;
/* A tile for an application not in the Dock, to keep it (a drop). */
- (DockTile *)tileForApplicationURL:(NSURL *)url;
- (void)removeTile:(DockTile *)tile;
/* The preferences again, from the domain. */
- (void)reloadPreferences;
/* Erases what the Trash holds. */
- (void)emptyTrash;
/* A window minimized into the Dock, shown by its image (nil: its owner's
   icon); the tile's index after the change. */
- (NSUInteger)addMinimizedWindow:(uint32_t)windowID owner:(pid_t)pid image:(nullable NSImage *)image;
- (nullable DockTile *)tileForMinimizedWindow:(uint32_t)windowID;
- (void)removeMinimizedWindow:(uint32_t)windowID;
/* The badge an app put on its tile (N2), from notificationd; nil if none. */
- (nullable NSString *)badgeForTile:(DockTile *)tile;
@end

/* The persistent-apps preference, in the reference format: tile-data with
   file-data's _CFURLString. Exposed for the tests. */
NSArray<NSURL *> *DockPersistentAppURLs(NSArray *_Nullable preference);
NSDictionary *DockPersistentAppEntry(NSURL *url);
/* The user's Trash folder (~/.Trash). */
NSURL *DockTrashURL(void);

NS_ASSUME_NONNULL_END
