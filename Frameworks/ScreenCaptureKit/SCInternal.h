/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* What ScreenCaptureKit's files share among themselves (not installed). */

#import <ScreenCaptureKit/ScreenCaptureKit.h>

NS_ASSUME_NONNULL_BEGIN

__attribute__((visibility("hidden"))) NSError *SCMakeError(SCStreamErrorCode code, NSString *description);
/* Pixels per point of a display (1 when unknown). */
__attribute__((visibility("hidden"))) float SCDisplayPointPixelScale(CGDirectDisplayID display);

@interface SCRunningApplication ()
- (instancetype)_initWithProcessID:(pid_t)pid
                  bundleIdentifier:(NSString *)bundleIdentifier
                   applicationName:(NSString *)name;
@end

@interface SCWindow ()
/* One of CGWindowListCopyWindowInfo's dictionaries. */
- (nullable instancetype)_initWithInfo:(NSDictionary *)info
                           application:(nullable SCRunningApplication *)application
                                active:(BOOL)active;
@end

@interface SCDisplay ()
- (instancetype)_initWithDisplayID:(CGDirectDisplayID)displayID;
@end

@interface SCShareableContentInfo ()
- (instancetype)_initWithStyle:(SCShareableContentStyle)style
               pointPixelScale:(float)scale
                   contentRect:(CGRect)rect;
@end

@interface SCShareableContent ()
/* The snapshot, now, on the caller's thread. */
+ (nullable SCShareableContent *)_contentWithOptions:(CGWindowListOption)options
                                      relativeToWindow:(CGWindowID)window
                                     currentProcessOnly:(BOOL)currentProcessOnly
                                                  error:(NSError **)error;
@end

@interface SCContentFilter ()
/* The content as it is now, one pixel per point, and its size in points.
   *exists is NO when its window or display is gone; the image may be NULL
   while it exists (a window with nothing drawn yet). */
- (nullable CGImageRef)_copyContentImageWithSize:(CGSize *)size exists:(BOOL *)exists CF_RETURNS_RETAINED;
@end

@interface SCStream ()
/* How many streams are capturing now, in the whole process. */
+ (NSInteger)_runningStreamCount;
@end

NS_ASSUME_NONNULL_END
