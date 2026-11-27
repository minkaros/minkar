/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Custom content: labelled values an element offers assistive apps
 * (API 20), shown or spoken as their importance says.
 */

#import <Foundation/Foundation.h>

typedef NS_ENUM(NSUInteger, AXCustomContentImportance) {
    AXCustomContentImportanceDefault = 0,
    AXCustomContentImportanceHigh = 1
};

API_AVAILABLE(macos(11.0))
@interface AXCustomContent : NSObject <NSCopying, NSSecureCoding>
+ (instancetype)customContentWithLabel:(NSString *)label value:(NSString *)value;
+ (instancetype)customContentWithAttributedLabel:(NSAttributedString *)label attributedValue:(NSAttributedString *)value;
- (instancetype)init NS_UNAVAILABLE;
+ (instancetype)new NS_UNAVAILABLE;
@property(readonly) NSString *label;
@property(readonly) NSAttributedString *attributedLabel;
@property(readonly) NSString *value;
@property(readonly) NSAttributedString *attributedValue;
@property AXCustomContentImportance importance;
@end

API_AVAILABLE(macos(11.0))
@protocol AXCustomContentProvider <NSObject>
@property(copy, nullable) NSArray<AXCustomContent *> *accessibilityCustomContent;
@end
