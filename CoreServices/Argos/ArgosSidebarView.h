/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@class ArgosSidebarView;

@protocol ArgosSidebarViewDelegate <NSObject>
- (void)sidebarView:(ArgosSidebarView *)view chooseURL:(NSURL *)url;
@end

/* The sidebar: Favorites (Applications, Desktop, Documents, Downloads, the
   home folder) and Locations (the system volume), as on the reference
   platform. The folder shown is marked. */
@interface ArgosSidebarView : NSView
@property (nonatomic, weak, nullable) id<ArgosSidebarViewDelegate> delegate;
@property (nonatomic, copy, nullable) NSURL *currentURL;
@end

NS_ASSUME_NONNULL_END
