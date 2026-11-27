/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* What can be captured: the displays, the windows and the applications
   that own them, as one snapshot. */

#import <Foundation/Foundation.h>
#import <CoreGraphics/CoreGraphics.h>

NS_ASSUME_NONNULL_BEGIN

@class SCContentFilter;

typedef NS_ENUM(NSInteger, SCShareableContentStyle) {
    SCShareableContentStyleNone = 0,
    SCShareableContentStyleWindow = 1,
    SCShareableContentStyleDisplay = 2,
    SCShareableContentStyleApplication = 3,
};

@interface SCRunningApplication : NSObject
@property (readonly) NSString *bundleIdentifier;
@property (readonly) NSString *applicationName;
@property (readonly) pid_t processID;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

@interface SCWindow : NSObject
@property (readonly) CGWindowID windowID;
@property (readonly) CGRect frame;
@property (readonly, nullable) NSString *title;
@property (readonly) NSInteger windowLayer;
@property (readonly, nullable) SCRunningApplication *owningApplication;
@property (readonly, getter=isOnScreen) BOOL onScreen;
@property (readonly, getter=isActive) BOOL active;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

@interface SCDisplay : NSObject
@property (readonly) CGDirectDisplayID displayID;
@property (readonly) NSInteger width;
@property (readonly) NSInteger height;
@property (readonly) CGRect frame;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

/* How a filter's content is laid out: its style, its rectangle in points
   and the pixels per point of its display. */
@interface SCShareableContentInfo : NSObject
@property (readonly) SCShareableContentStyle style;
@property (readonly) float pointPixelScale;
@property (readonly) CGRect contentRect;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@end

@interface SCShareableContent : NSObject
@property (readonly) NSArray<SCWindow *> *windows;
@property (readonly) NSArray<SCDisplay *> *displays;
@property (readonly) NSArray<SCRunningApplication *> *applications;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/* The handlers are called on a queue of the framework's, not the caller's. */
+ (void)getShareableContentWithCompletionHandler:
    (void (^)(SCShareableContent *_Nullable shareableContent, NSError *_Nullable error))completionHandler;
+ (void)getCurrentProcessShareableContentWithCompletionHandler:
    (void (^)(SCShareableContent *_Nullable shareableContent, NSError *_Nullable error))completionHandler;
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                               onScreenWindowsOnly:(BOOL)onScreenWindowsOnly
                                 completionHandler:(void (^)(SCShareableContent *_Nullable shareableContent,
                                                             NSError *_Nullable error))completionHandler;
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                          onScreenWindowsOnlyBelowWindow:(SCWindow *)window
                                 completionHandler:(void (^)(SCShareableContent *_Nullable shareableContent,
                                                             NSError *_Nullable error))completionHandler;
+ (void)getShareableContentExcludingDesktopWindows:(BOOL)excludeDesktopWindows
                          onScreenWindowsOnlyAboveWindow:(SCWindow *)window
                                 completionHandler:(void (^)(SCShareableContent *_Nullable shareableContent,
                                                             NSError *_Nullable error))completionHandler;

+ (SCShareableContentInfo *)infoForFilter:(SCContentFilter *)filter;
@end

NS_ASSUME_NONNULL_END
