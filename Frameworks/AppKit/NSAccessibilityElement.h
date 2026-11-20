// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <AppKit/NSAccessibility.h>
#import <AppKit/NSAccessibilityProtocols.h>

NS_ASSUME_NONNULL_BEGIN

/* An accessibility element that is not a view: a part of one (a cell, a
   page region) given its role, frame, label and parent. */
@interface NSAccessibilityElement : NSObject <NSAccessibilityElement>
+ (id)accessibilityElementWithRole:(NSString *)role frame:(NSRect)frame label:(nullable NSString *)label parent:(nullable id)parent;
- (void)accessibilityAddChildElement:(NSAccessibilityElement *)childElement;
@property NSRect accessibilityFrameInParentSpace;
@end

NS_ASSUME_NONNULL_END
