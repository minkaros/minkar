/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/Foundation.h>
#import <AppKit/AppKitExport.h>
#import <AppKit/NSView.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSLayoutAnchor.h>

@class NSLayoutGuide;

NS_ASSUME_NONNULL_BEGIN

typedef float NSLayoutPriority NS_TYPED_EXTENSIBLE_ENUM;
static const NSLayoutPriority NSLayoutPriorityRequired = 1000;
static const NSLayoutPriority NSLayoutPriorityDefaultHigh = 750;
static const NSLayoutPriority NSLayoutPriorityDragThatCanResizeWindow = 510;
static const NSLayoutPriority NSLayoutPriorityWindowSizeStayPut = 500;
static const NSLayoutPriority NSLayoutPriorityDragThatCannotResizeWindow = 490;
static const NSLayoutPriority NSLayoutPriorityDefaultLow = 250;
static const NSLayoutPriority NSLayoutPriorityFittingSizeCompression = 50;

typedef NS_ENUM(NSInteger, NSLayoutConstraintOrientation) {
    NSLayoutConstraintOrientationHorizontal = 0,
    NSLayoutConstraintOrientationVertical = 1,
};

typedef NS_ENUM(NSInteger, NSLayoutRelation) {
    NSLayoutRelationLessThanOrEqual = -1,
    NSLayoutRelationEqual = 0,
    NSLayoutRelationGreaterThanOrEqual = 1,
};

typedef NS_ENUM(NSInteger, NSLayoutAttribute) {
    NSLayoutAttributeLeft = 1,
    NSLayoutAttributeRight,
    NSLayoutAttributeTop,
    NSLayoutAttributeBottom,
    NSLayoutAttributeLeading,
    NSLayoutAttributeTrailing,
    NSLayoutAttributeWidth,
    NSLayoutAttributeHeight,
    NSLayoutAttributeCenterX,
    NSLayoutAttributeCenterY,
    NSLayoutAttributeLastBaseline,
    NSLayoutAttributeBaseline = NSLayoutAttributeLastBaseline,
    NSLayoutAttributeFirstBaseline,
    NSLayoutAttributeNotAnAttribute = 0,
};

typedef NS_OPTIONS(NSUInteger, NSLayoutFormatOptions) {
    NSLayoutFormatAlignAllLeft = (1 << NSLayoutAttributeLeft),
    NSLayoutFormatAlignAllRight = (1 << NSLayoutAttributeRight),
    NSLayoutFormatAlignAllTop = (1 << NSLayoutAttributeTop),
    NSLayoutFormatAlignAllBottom = (1 << NSLayoutAttributeBottom),
    NSLayoutFormatAlignAllLeading = (1 << NSLayoutAttributeLeading),
    NSLayoutFormatAlignAllTrailing = (1 << NSLayoutAttributeTrailing),
    NSLayoutFormatAlignAllCenterX = (1 << NSLayoutAttributeCenterX),
    NSLayoutFormatAlignAllCenterY = (1 << NSLayoutAttributeCenterY),
    NSLayoutFormatAlignAllLastBaseline = (1 << NSLayoutAttributeLastBaseline),
    NSLayoutFormatAlignAllFirstBaseline = (1 << NSLayoutAttributeFirstBaseline),
    NSLayoutFormatAlignAllBaseline = NSLayoutFormatAlignAllLastBaseline,
    NSLayoutFormatAlignmentMask = 0xFFFF,
    NSLayoutFormatDirectionLeadingToTrailing = 0 << 16,
    NSLayoutFormatDirectionLeftToRight = 1 << 16,
    NSLayoutFormatDirectionRightToLeft = 2 << 16,
    NSLayoutFormatDirectionMask = 0x3 << 16,
};

// A relation between attributes of two items (views or layout guides):
// first.attribute relation multiplier * second.attribute + constant.
@interface NSLayoutConstraint : NSObject {
    id _firstItem;
    id _secondItem;
    NSLayoutAttribute _firstAttribute;
    NSLayoutAttribute _secondAttribute;
    NSLayoutRelation _relation;
    CGFloat _multiplier;
    CGFloat _constant;
    NSLayoutPriority _priority;
    NSString *_identifier;
    BOOL _active;
    BOOL _shouldBeArchived;
    void *_engineConstraint;
}

+ (NSArray<NSLayoutConstraint *> *)constraintsWithVisualFormat:(NSString *)format options:(NSLayoutFormatOptions)opts metrics:(nullable NSDictionary<NSString *, id> *)metrics views:(NSDictionary<NSString *, id> *)views;
+ (instancetype)constraintWithItem:(id)view1 attribute:(NSLayoutAttribute)attr1 relatedBy:(NSLayoutRelation)relation toItem:(nullable id)view2 attribute:(NSLayoutAttribute)attr2 multiplier:(CGFloat)multiplier constant:(CGFloat)c;
+ (void)activateConstraints:(NSArray<NSLayoutConstraint *> *)constraints;
+ (void)deactivateConstraints:(NSArray<NSLayoutConstraint *> *)constraints;

@property NSLayoutPriority priority;
@property BOOL shouldBeArchived;
@property (nullable, readonly, assign) id firstItem;
@property (nullable, readonly, assign) id secondItem;
@property (readonly) NSLayoutAttribute firstAttribute;
@property (readonly) NSLayoutAttribute secondAttribute;
@property (readonly, copy) NSLayoutAnchor *firstAnchor;
@property (nullable, readonly, copy) NSLayoutAnchor *secondAnchor;
@property (readonly) NSLayoutRelation relation;
@property (readonly) CGFloat multiplier;
@property CGFloat constant;
@property (getter=isActive) BOOL active;
@property (nullable, copy) NSString *identifier;

@end

// A dictionary of the variables named, by their names: for the visual
// format language's views (NSDictionaryOfVariableBindings(a, b)).
#define NSDictionaryOfVariableBindings(...) _NSDictionaryOfVariableBindings(@"" # __VA_ARGS__, __VA_ARGS__, nil)
APPKIT_EXPORT NSDictionary<NSString *, id> *_NSDictionaryOfVariableBindings(NSString *commaSeparatedKeysString, _Nullable id firstValue, ...);

@interface NSView (NSConstraintBasedLayoutInstallingConstraints)
@property (readonly, copy) NSArray<__kindof NSLayoutConstraint *> *constraints;
- (void)addConstraint:(NSLayoutConstraint *)constraint;
- (void)addConstraints:(NSArray<NSLayoutConstraint *> *)constraints;
- (void)removeConstraint:(NSLayoutConstraint *)constraint;
- (void)removeConstraints:(NSArray<NSLayoutConstraint *> *)constraints;
@end

@interface NSView (NSConstraintBasedLayoutCoreMethods)
- (void)updateConstraintsForSubtreeIfNeeded;
- (void)updateConstraints;
@property BOOL needsUpdateConstraints;
- (void)layoutSubtreeIfNeeded;
- (void)layout;
@property BOOL needsLayout;
@end

@interface NSView (NSConstraintBasedCompatibility)
@property BOOL translatesAutoresizingMaskIntoConstraints;
@property (class, readonly) BOOL requiresConstraintBasedLayout;
@end

APPKIT_EXPORT const CGFloat NSViewNoIntrinsicMetric;

@interface NSView (NSConstraintBasedLayoutLayering)
- (NSRect)alignmentRectForFrame:(NSRect)frame;
- (NSRect)frameForAlignmentRect:(NSRect)alignmentRect;
@property (readonly) NSEdgeInsets alignmentRectInsets;
@property (readonly) CGFloat firstBaselineOffsetFromTop;
@property (readonly) CGFloat lastBaselineOffsetFromBottom;
@property (readonly) CGFloat baselineOffsetFromBottom;
@property (readonly) NSSize intrinsicContentSize;
- (void)invalidateIntrinsicContentSize;
- (NSLayoutPriority)contentHuggingPriorityForOrientation:(NSLayoutConstraintOrientation)orientation;
- (void)setContentHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation;
- (NSLayoutPriority)contentCompressionResistancePriorityForOrientation:(NSLayoutConstraintOrientation)orientation;
- (void)setContentCompressionResistancePriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation;
@end

@interface NSView (NSConstraintBasedLayoutFittingSize)
@property (readonly) NSSize fittingSize;
@end

@interface NSView (NSConstraintBasedLayoutDebugging)
- (NSArray<NSLayoutConstraint *> *)constraintsAffectingLayoutForOrientation:(NSLayoutConstraintOrientation)orientation;
@property (readonly) BOOL hasAmbiguousLayout;
@end

@interface NSWindow (NSConstraintBasedLayoutCoreMethods)
- (void)updateConstraintsIfNeeded;
- (void)layoutIfNeeded;
@end

NS_ASSUME_NONNULL_END
