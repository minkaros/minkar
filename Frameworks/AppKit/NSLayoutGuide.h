/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/Foundation.h>
#import <AppKit/NSView.h>
#import <AppKit/NSLayoutAnchor.h>
#import <AppKit/NSLayoutConstraint.h>

NS_ASSUME_NONNULL_BEGIN

// A rectangle of a view's layout that is not a view: constraints place it.
@interface NSLayoutGuide : NSObject <NSCoding> {
    NSView *_owningView;
    NSString *_identifier;
    NSRect _frame;
    NSMutableDictionary *_anchors;
}

@property (readonly) NSRect frame;
@property (nullable, assign) NSView *owningView;
@property (copy) NSString *identifier;

@property (readonly, strong) NSLayoutXAxisAnchor *leadingAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *trailingAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *leftAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *rightAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *topAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *bottomAnchor;
@property (readonly, strong) NSLayoutDimension *widthAnchor;
@property (readonly, strong) NSLayoutDimension *heightAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *centerXAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *centerYAnchor;

@property (readonly) BOOL hasAmbiguousLayout;
- (NSArray<NSLayoutConstraint *> *)constraintsAffectingLayoutForOrientation:(NSLayoutConstraintOrientation)orientation;

@end

@interface NSView (NSLayoutGuideSupport)
- (void)addLayoutGuide:(NSLayoutGuide *)guide;
- (void)removeLayoutGuide:(NSLayoutGuide *)guide;
@property (readonly, copy) NSArray<NSLayoutGuide *> *layoutGuides;
@end

@interface NSView (NSViewLayoutAnchors)
@property (readonly, strong) NSLayoutXAxisAnchor *leadingAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *trailingAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *leftAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *rightAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *topAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *bottomAnchor;
@property (readonly, strong) NSLayoutDimension *widthAnchor;
@property (readonly, strong) NSLayoutDimension *heightAnchor;
@property (readonly, strong) NSLayoutXAxisAnchor *centerXAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *centerYAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *firstBaselineAnchor;
@property (readonly, strong) NSLayoutYAxisAnchor *lastBaselineAnchor;
@end

NS_ASSUME_NONNULL_END
