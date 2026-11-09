// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The shared preview panel: it shows the items its data source gives
// (images, text, or else the item's icon and name), and is controlled by
// the first responder that accepts it.

#import <AppKit/AppKit.h>

NS_ASSUME_NONNULL_BEGIN

@protocol QLPreviewItem <NSObject>
@property (nullable, readonly) NSURL *previewItemURL;
@optional
@property (nullable, readonly) NSString *previewItemTitle;
@property (nullable, readonly) id previewItemDisplayState;
@end

@interface NSURL (QLPreviewConvenienceAdditions) <QLPreviewItem>
@end

@class QLPreviewPanel;

@protocol QLPreviewPanelDataSource <NSObject>
- (NSInteger)numberOfPreviewItemsInPreviewPanel:(QLPreviewPanel *)panel;
- (nullable id<QLPreviewItem>)previewPanel:(QLPreviewPanel *)panel previewItemAtIndex:(NSInteger)index;
@end

@protocol QLPreviewPanelDelegate <NSObject, NSWindowDelegate>
@optional
- (BOOL)previewPanel:(QLPreviewPanel *)panel handleEvent:(NSEvent *)event;
- (NSRect)previewPanel:(QLPreviewPanel *)panel sourceFrameOnScreenForPreviewItem:(id<QLPreviewItem>)item;
@end

@interface NSObject (QLPreviewPanelController)
- (BOOL)acceptsPreviewPanelControl:(QLPreviewPanel *)panel;
- (void)beginPreviewPanelControl:(QLPreviewPanel *)panel;
- (void)endPreviewPanelControl:(QLPreviewPanel *)panel;
@end

@interface QLPreviewPanel : NSPanel
+ (QLPreviewPanel *)sharedPreviewPanel;
+ (BOOL)sharedPreviewPanelExists;
@property (nullable, readonly, strong) id currentController;
@property (nullable, weak) id<QLPreviewPanelDataSource> dataSource;
@property NSInteger currentPreviewItemIndex;
@property (nullable, readonly) id<QLPreviewItem> currentPreviewItem;
@property (nullable, strong) id displayState;
- (void)updateController;
- (void)reloadData;
- (void)refreshCurrentPreviewItem;
- (BOOL)enterFullScreenMode:(nullable NSScreen *)screen withOptions:(nullable NSDictionary *)options;
- (void)exitFullScreenModeWithOptions:(nullable NSDictionary *)options;
@property (readonly, getter=isInFullScreenMode) BOOL inFullScreenMode;
@end

NS_ASSUME_NONNULL_END
