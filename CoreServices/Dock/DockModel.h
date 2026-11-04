/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The Dock's model (Dock-Kraz.md, section 4): its tiles, as plain data.
 * Views only display it.
 */

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, DockTileKind) {
    DockTileKindPersistentApp,   /* kept in the Dock (persistent-apps) */
    DockTileKindRunningApp,      /* running, not kept */
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

extern NSNotificationName const DockModelDidChangeNotification;

@interface DockModel : NSObject
/* Persistent tiles first, then the running apps that are not kept. */
@property (nonatomic, readonly, copy) NSArray<DockTile *> *tiles;
/* Index of the first running-only tile (the separator goes before it). */
@property (nonatomic, readonly) NSUInteger firstRunningIndex;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
/* Launches the tile's app, or activates it when it runs. */
- (BOOL)openTile:(DockTile *)tile;
@end

/* The persistent-apps preference, in the reference format: tile-data with
   file-data's _CFURLString. Exposed for the tests. */
NSArray<NSURL *> *DockPersistentAppURLs(NSArray *_Nullable preference);
NSDictionary *DockPersistentAppEntry(NSURL *url);

NS_ASSUME_NONNULL_END
