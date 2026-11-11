/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The roles an accessible object takes on (API 10.10): protocols naming
 * the properties of each, which NSObject's accessibility properties
 * (NSAccessibility.h) provide.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSGeometry.h>
#import <Foundation/NSRange.h>

@class NSString, NSArray, NSAttributedString;

NS_ASSUME_NONNULL_BEGIN

@protocol NSAccessibilityElement <NSObject>
@required
- (NSRect)accessibilityFrame;
- (nullable id)accessibilityParent;
@optional
- (BOOL)isAccessibilityFocused;
- (NSString *)accessibilityIdentifier;
@end

@protocol NSAccessibilityGroup <NSAccessibilityElement>
@end

@protocol NSAccessibilityButton <NSAccessibilityElement>
@required
- (nullable NSString *)accessibilityLabel;
- (BOOL)accessibilityPerformPress;
@end

@protocol NSAccessibilitySwitch <NSAccessibilityButton>
@required
- (nullable NSString *)accessibilityValue;
@optional
- (BOOL)accessibilityPerformIncrement;
- (BOOL)accessibilityPerformDecrement;
@end

@protocol NSAccessibilityRadioButton <NSAccessibilityButton>
@required
- (nullable NSNumber *)accessibilityValue;
@end

@protocol NSAccessibilityCheckBox <NSAccessibilityButton>
@required
- (nullable NSNumber *)accessibilityValue;
@end

@protocol NSAccessibilityStaticText <NSAccessibilityElement>
@required
- (nullable NSString *)accessibilityValue;
@optional
- (nullable NSAttributedString *)accessibilityAttributedStringForRange:(NSRange)range;
- (NSRange)accessibilityVisibleCharacterRange;
@end

@protocol NSAccessibilityNavigableStaticText <NSAccessibilityStaticText>
@required
- (nullable NSString *)accessibilityStringForRange:(NSRange)range;
- (NSInteger)accessibilityLineForIndex:(NSInteger)index;
- (NSRange)accessibilityRangeForLine:(NSInteger)lineNumber;
- (NSRect)accessibilityFrameForRange:(NSRange)range;
@end

@protocol NSAccessibilityProgressIndicator <NSAccessibilityGroup>
@required
- (nullable NSNumber *)accessibilityValue;
@end

@protocol NSAccessibilityStepper <NSAccessibilityElement>
@required
- (nullable NSString *)accessibilityLabel;
- (BOOL)accessibilityPerformIncrement;
- (BOOL)accessibilityPerformDecrement;
@optional
- (nullable id)accessibilityValue;
@end

@protocol NSAccessibilitySlider <NSAccessibilityElement>
@required
- (nullable NSString *)accessibilityLabel;
- (nullable id)accessibilityValue;
- (BOOL)accessibilityPerformIncrement;
- (BOOL)accessibilityPerformDecrement;
@end

@protocol NSAccessibilityImage <NSAccessibilityElement>
@required
- (nullable NSString *)accessibilityLabel;
@end

@protocol NSAccessibilityContainsTransientUI <NSAccessibilityElement>
@required
- (BOOL)accessibilityPerformShowAlternateUI;
- (BOOL)accessibilityPerformShowDefaultUI;
- (BOOL)isAccessibilityAlternateUIVisible;
@end

@protocol NSAccessibilityTable <NSAccessibilityGroup>
@required
- (nullable NSString *)accessibilityLabel;
- (nullable NSArray *)accessibilityRows;
@optional
- (nullable NSArray *)accessibilitySelectedRows;
- (nullable NSArray *)accessibilityVisibleRows;
- (nullable NSArray *)accessibilityColumns;
- (nullable NSArray *)accessibilityVisibleColumns;
- (nullable NSArray *)accessibilitySelectedColumns;
- (nullable NSString *)accessibilityHeaderGroup;
- (nullable NSArray *)accessibilitySelectedCells;
- (nullable NSArray *)accessibilityVisibleCells;
- (nullable NSArray *)accessibilityRowHeaderUIElements;
- (nullable NSArray *)accessibilityColumnHeaderUIElements;
@end

@protocol NSAccessibilityOutline <NSAccessibilityTable>
@end

@protocol NSAccessibilityList <NSAccessibilityTable>
@end

@protocol NSAccessibilityRow <NSAccessibilityGroup>
@required
- (NSInteger)accessibilityIndex;
@optional
- (NSInteger)accessibilityDisclosureLevel;
@end

@protocol NSAccessibilityLayoutArea <NSAccessibilityGroup>
@required
- (NSString *)accessibilityLabel;
- (nullable NSArray *)accessibilityChildren;
- (nullable NSArray *)accessibilitySelectedChildren;
- (id)accessibilityFocusedUIElement;
@end

@protocol NSAccessibilityLayoutItem <NSAccessibilityGroup>
@optional
- (void)setAccessibilityFrame:(NSRect)frame;
@end

NS_ASSUME_NONNULL_END
