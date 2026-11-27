/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An item to share with the title and image the share sheet shows for
 * it (API 23).
 */

#import <Foundation/Foundation.h>

@class NSImage;

@protocol NSPreviewRepresentableActivityItem <NSObject>
@property(readonly, strong) id item;
@optional
@property(readonly, copy) NSString *title;
@property(readonly, strong) NSItemProvider *imageProvider;
@property(readonly, strong) NSItemProvider *iconProvider;
@end

API_AVAILABLE(macos(13.0))
@interface NSPreviewRepresentingActivityItem : NSObject <NSPreviewRepresentableActivityItem> {
    id _item;
    NSString *_title;
    NSItemProvider *_imageProvider;
    NSItemProvider *_iconProvider;
}

- (instancetype)initWithItem:(id)item title:(NSString *)title image:(NSImage *)image icon:(NSImage *)icon;
- (instancetype)initWithItem:(id)item title:(NSString *)title imageProvider:(NSItemProvider *)imageProvider
                iconProvider:(NSItemProvider *)iconProvider;
- (instancetype)init NS_UNAVAILABLE;
@end
