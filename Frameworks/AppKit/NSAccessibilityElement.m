// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <AppKit/NSAccessibilityElement.h>
#import <AppKit/NSView.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSValue.h>

@implementation NSAccessibilityElement {
    NSRect _frameInParentSpace;
    BOOL _hasFrameInParentSpace;
    BOOL _hasElementFlag;
    BOOL _element;
}

+ (id)accessibilityElementWithRole:(NSString *)role frame:(NSRect)frame label:(NSString *)label parent:(id)parent {
    NSAccessibilityElement *result = [[[self alloc] init] autorelease];

    [result setAccessibilityRole:role];
    [result setAccessibilityFrame:frame];
    [result setAccessibilityLabel:label];
    [result setAccessibilityParent:parent];
    return result;
}

/* An element is one unless told otherwise, as on the reference. */
- (BOOL)isAccessibilityElement {
    return _hasElementFlag ? _element : YES;
}

- (void)setAccessibilityElement:(BOOL)flag {
    _hasElementFlag = YES;
    _element = flag;
}

/* Added as a child, with this element as its parent; children are none
   until one is added. */
- (void)accessibilityAddChildElement:(NSAccessibilityElement *)child {
    NSArray *children = [self accessibilityChildren];

    if (child == nil)
        return;
    [self setAccessibilityChildren:(children != nil) ? [children arrayByAddingObject:child] : [NSArray arrayWithObject:child]];
    [child setAccessibilityParent:self];
}

- (NSRect)accessibilityFrameInParentSpace {
    return _hasFrameInParentSpace ? _frameInParentSpace : NSZeroRect;
}

- (void)setAccessibilityFrameInParentSpace:(NSRect)frame {
    _frameInParentSpace = frame;
    _hasFrameInParentSpace = YES;
}

/* A frame in its parent's space is put on the screen through the parent:
   a view's own coordinates, or another element's frame. */
- (NSRect)accessibilityFrame {
    if (_hasFrameInParentSpace) {
        id parent = [self accessibilityParent];

        if ([parent isKindOfClass:[NSView class]])
            return NSAccessibilityFrameInView((NSView *)parent, _frameInParentSpace);
        if ([parent respondsToSelector:@selector(accessibilityFrame)]) {
            NSRect parentFrame = [parent accessibilityFrame];
            return NSOffsetRect(_frameInParentSpace, NSMinX(parentFrame), NSMinY(parentFrame));
        }
        return _frameInParentSpace;
    }
    return [super accessibilityFrame];
}

@end
