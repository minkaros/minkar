/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <AppKit/NSLayoutConstraint.h>
#import <AppKit/NSLayoutAnchor.h>
#import <AppKit/NSLayoutGuide.h>

/* Constraint-based layout: what the classes share. A view's layout state
 * (its constraints, its flags and priorities) is kept with it as an
 * associated object; a layout pass solves every constraint of a window's
 * views at once (NSLayoutEngine). */

@interface NSViewLayoutState : NSObject {
@public
    NSMutableArray *constraints;
    NSMutableArray *layoutGuides;
    NSMutableDictionary *anchors;
    BOOL translatesAutoresizingMask;
    BOOL needsLayout;
    BOOL needsUpdateConstraints;
    NSLayoutPriority hugging[2];
    NSLayoutPriority compressionResistance[2];
}
@end

NSViewLayoutState *NSViewLayoutStateFor(NSView *view);
NSViewLayoutState *NSViewLayoutStateIfAny(NSView *view);

// A view or a layout guide: the view whose bounds place it, and so on.
NSView *NSLayoutItemView(id item);

// Marks the window (or the top view) of an item as needing a layout pass.
void NSLayoutSetNeedsLayout(id item);

@interface NSLayoutConstraint (private)
- (NSString *)_engineDescription;
- (void)_setActiveFlag:(BOOL)flag;
@end

@interface NSLayoutAnchor (private)
- (instancetype)_initWithItem:(id)item attribute:(NSLayoutAttribute)attribute;
- (NSLayoutAttribute)_attribute;
@end

// Anchors of an item, made once each.
id NSLayoutAnchorFor(id item, NSLayoutAttribute attribute, Class kind);

@interface NSLayoutGuide (private)
- (void)_setFrame:(NSRect)frame;
@end

// What NSView tells layout: a new size, a hidden or shown view, a view
// added to a superview. Cheap when no constraints are in use.
void NSLayoutViewDidResize(NSView *view);
void NSLayoutViewDidChangeVisibility(NSView *view);
void NSLayoutViewDidMove(NSView *view);

// The layout pass over root's subtree; fitting: the smallest size the
// constraints allow, with root's own size free (fittingSize).
void NSLayoutEngineLayout(NSView *root);
NSSize NSLayoutEngineFittingSize(NSView *view);

// Spacing the visual format language uses for "-": between siblings, and
// to the superview's edge.
#define NSLayoutStandardSpacing 8.0
#define NSLayoutStandardSuperviewSpacing 20.0
