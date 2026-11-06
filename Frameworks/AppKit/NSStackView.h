/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <AppKit/NSView.h>
#import <AppKit/NSLayoutConstraint.h>

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSUserInterfaceLayoutOrientation) {
    NSUserInterfaceLayoutOrientationHorizontal = 0,
    NSUserInterfaceLayoutOrientationVertical = 1,
};

typedef NS_ENUM(NSInteger, NSStackViewGravity) {
    NSStackViewGravityTop = 1,
    NSStackViewGravityLeading = 1,
    NSStackViewGravityCenter = 2,
    NSStackViewGravityBottom = 3,
    NSStackViewGravityTrailing = 3,
};

typedef NS_ENUM(NSInteger, NSStackViewDistribution) {
    NSStackViewDistributionGravityAreas = -1,
    NSStackViewDistributionFill = 0,
    NSStackViewDistributionFillEqually,
    NSStackViewDistributionFillProportionally,
    NSStackViewDistributionEqualSpacing,
    NSStackViewDistributionEqualCentering,
};

typedef float NSStackViewVisibilityPriority NS_TYPED_EXTENSIBLE_ENUM;
static const NSStackViewVisibilityPriority NSStackViewVisibilityPriorityMustHold = 1000;
static const NSStackViewVisibilityPriority NSStackViewVisibilityPriorityDetachOnlyIfNecessary = 900;
static const NSStackViewVisibilityPriority NSStackViewVisibilityPriorityNotVisible = 0;

APPKIT_EXPORT const CGFloat NSStackViewSpacingUseDefault;

@protocol NSStackViewDelegate;

// A row or column of views, kept in place by constraints.
@interface NSStackView : NSView {
    id _delegate;
    NSUserInterfaceLayoutOrientation _orientation;
    NSLayoutAttribute _alignment;
    NSEdgeInsets _edgeInsets;
    NSStackViewDistribution _distribution;
    CGFloat _spacing;
    BOOL _detachesHiddenViews;
    NSMutableArray *_arrangedSubviews;
    NSMutableArray *_gravities;
    NSMutableDictionary *_customSpacing;
    NSMutableDictionary *_visibilityPriorities;
    NSMutableArray *_stackConstraints;
    NSLayoutPriority _hugging[2];
    NSLayoutPriority _clippingResistance[2];
}

+ (instancetype)stackViewWithViews:(NSArray<NSView *> *)views;

@property (nullable, assign) id<NSStackViewDelegate> delegate;
@property NSUserInterfaceLayoutOrientation orientation;
@property NSLayoutAttribute alignment;
@property NSEdgeInsets edgeInsets;
@property NSStackViewDistribution distribution;
@property CGFloat spacing;
@property BOOL detachesHiddenViews;

- (void)setCustomSpacing:(CGFloat)spacing afterView:(NSView *)view;
- (CGFloat)customSpacingAfterView:(NSView *)view;

@property (readonly, copy) NSArray<__kindof NSView *> *arrangedSubviews;
- (void)addArrangedSubview:(NSView *)view;
- (void)insertArrangedSubview:(NSView *)view atIndex:(NSInteger)index;
- (void)removeArrangedSubview:(NSView *)view;

@property (readonly, copy) NSArray<__kindof NSView *> *detachedViews;
- (void)setVisibilityPriority:(NSStackViewVisibilityPriority)priority forView:(NSView *)view;
- (NSStackViewVisibilityPriority)visibilityPriorityForView:(NSView *)view;

- (NSLayoutPriority)clippingResistancePriorityForOrientation:(NSLayoutConstraintOrientation)orientation;
- (void)setClippingResistancePriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation;
- (NSLayoutPriority)huggingPriorityForOrientation:(NSLayoutConstraintOrientation)orientation;
- (void)setHuggingPriority:(NSLayoutPriority)priority forOrientation:(NSLayoutConstraintOrientation)orientation;

// Gravity areas (the older API): views by gravity.
- (void)addView:(NSView *)view inGravity:(NSStackViewGravity)gravity;
- (void)insertView:(NSView *)view atIndex:(NSUInteger)index inGravity:(NSStackViewGravity)gravity;
- (void)removeView:(NSView *)view;
- (NSArray<__kindof NSView *> *)viewsInGravity:(NSStackViewGravity)gravity;
- (void)setViews:(NSArray<NSView *> *)views inGravity:(NSStackViewGravity)gravity;
@property (readonly, copy) NSArray<__kindof NSView *> *views;

@end

@protocol NSStackViewDelegate <NSObject>
@optional
- (void)stackView:(NSStackView *)stackView willDetachViews:(NSArray<NSView *> *)views;
- (void)stackView:(NSStackView *)stackView didReattachViews:(NSArray<NSView *> *)views;
@end

NS_ASSUME_NONNULL_END
