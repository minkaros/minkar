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
@property (nonatomic, readonly) DockTile *trashTile;
/* Whether the Trash holds anything (its icon shows it). */
@property (nonatomic, readonly) BOOL trashIsFull;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
/* Launches the tile's app, or activates it when it runs. */
- (BOOL)openTile:(DockTile *)tile;
/* Kept in the Dock or not (Keep in Dock, Remove from Dock). */
- (BOOL)isKept:(DockTile *)tile;
- (void)keepTile:(DockTile *)tile atIndex:(NSUInteger)index;
- (void)removeTile:(DockTile *)tile;
/* The preferences again, from the domain. */
- (void)reloadPreferences;
/* Erases what the Trash holds. */
- (void)emptyTrash;
@end

/* The persistent-apps preference, in the reference format: tile-data with
   file-data's _CFURLString. Exposed for the tests. */
NSArray<NSURL *> *DockPersistentAppURLs(NSArray *_Nullable preference);
NSDictionary *DockPersistentAppEntry(NSURL *url);
/* The user's Trash folder (~/.Trash). */
NSURL *DockTrashURL(void);

NS_ASSUME_NONNULL_END
