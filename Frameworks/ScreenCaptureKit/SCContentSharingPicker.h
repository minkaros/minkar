/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

/* The system's picker of what to share: a panel where the user chooses a
   display, windows or applications, which reaches the observers as a
   content filter. */

#import <Foundation/Foundation.h>
#import <ScreenCaptureKit/SCShareableContent.h>
#import <ScreenCaptureKit/SCStream.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, SCContentSharingPickerMode) {
    SCContentSharingPickerModeSingleWindow = 1 << 0,
    SCContentSharingPickerModeMultipleWindows = 1 << 1,
    SCContentSharingPickerModeSingleApplication = 1 << 2,
    SCContentSharingPickerModeMultipleApplications = 1 << 3,
    SCContentSharingPickerModeSingleDisplay = 1 << 4,
};

@interface SCContentSharingPickerConfiguration : NSObject <NSCopying>
@property SCContentSharingPickerMode allowedPickerModes;
/* Whether the user may change a running stream's content. */
@property BOOL allowsChangingSelectedContent;
@property (copy) NSArray<NSString *> *excludedBundleIDs;
@property (copy) NSArray<NSNumber *> *excludedWindowIDs;
@end

@class SCContentSharingPicker;

@protocol SCContentSharingPickerObserver <NSObject>
- (void)contentSharingPicker:(SCContentSharingPicker *)picker didCancelForStream:(nullable SCStream *)stream;
- (void)contentSharingPicker:(SCContentSharingPicker *)picker
         didUpdateWithFilter:(SCContentFilter *)filter
                   forStream:(nullable SCStream *)stream;
- (void)contentSharingPickerStartDidFailWithError:(NSError *)error;
@end

@interface SCContentSharingPicker : NSObject
@property (class, readonly) SCContentSharingPicker *sharedPicker;
/* A copy: set it back to change it. */
@property (copy) SCContentSharingPickerConfiguration *defaultConfiguration;
/* The most streams the picker may run at once; nil, no limit. */
@property (nullable, strong) NSNumber *maximumStreamCount;
/* The picker presents nothing while inactive. */
@property (getter=isActive) BOOL active;

- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;

/* Observers are not retained. */
- (void)addObserver:(id<SCContentSharingPickerObserver>)observer;
- (void)removeObserver:(id<SCContentSharingPickerObserver>)observer;
- (void)setConfiguration:(nullable SCContentSharingPickerConfiguration *)pickerConfig
               forStream:(SCStream *)stream;

- (void)present;
- (void)presentPickerUsingContentStyle:(SCShareableContentStyle)contentStyle;
/* A nil stream: the observers are told of the choice for no stream yet. */
- (void)presentPickerForStream:(nullable SCStream *)stream;
- (void)presentPickerForStream:(nullable SCStream *)stream usingContentStyle:(SCShareableContentStyle)contentStyle;
@end

NS_ASSUME_NONNULL_END
