/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The desktop (Dock-Argos.md, D1): one window per display at the desktop
 * level, with the display's picture (NSWorkspace's desktop image API) over
 * its fill color. It takes no clicks: they reach whatever is below.
 */

#import "DesktopWindow.h"

NSColor *DesktopDefaultColor(void)
{
    return [NSColor colorWithCalibratedRed:99 / 255.0 green:130 / 255.0 blue:145 / 255.0 alpha:1];
}

NSRect DesktopImageRect(NSSize image, NSRect bounds, NSImageScaling scaling, BOOL clipping)
{
    NSSize size = image;

    if (image.width <= 0 || image.height <= 0)
        return bounds;
    switch (scaling) {
    case NSImageScaleAxesIndependently:
        return bounds;
    case NSImageScaleNone:
        break;
    case NSImageScaleProportionallyDown:
    case NSImageScaleProportionallyUpOrDown:
    default: {
        CGFloat sx = NSWidth(bounds) / image.width, sy = NSHeight(bounds) / image.height;
        /* Clipping allowed: it fills the desktop; otherwise it fits. */
        CGFloat scale = clipping ? MAX(sx, sy) : MIN(sx, sy);
        if (scaling == NSImageScaleProportionallyDown)
            scale = MIN(scale, 1);
        size = NSMakeSize(image.width * scale, image.height * scale);
        break;
    }
    }
    return NSMakeRect(NSMidX(bounds) - size.width / 2, NSMidY(bounds) - size.height / 2,
                      size.width, size.height);
}

@interface DesktopView : NSView
@property (nonatomic, strong, nullable) NSImage *image;
@property (nonatomic, strong) NSColor *color;
@property (nonatomic) NSImageScaling scaling;
@property (nonatomic) BOOL clipping;
@end

@implementation DesktopView

- (BOOL)isOpaque
{
    return YES;
}

- (void)drawRect:(NSRect)dirty
{
    [self.color set];
    NSRectFill([self bounds]);
    if (self.image != nil)
        [self.image drawInRect:DesktopImageRect([self.image size], [self bounds], self.scaling, self.clipping)
                      fromRect:NSZeroRect operation:NSCompositeSourceOver fraction:1];
}

@end

@implementation DesktopWindow {
    NSScreen *_screen;
    DesktopView *_view;
}

- (instancetype)initWithScreen:(NSScreen *)screen
{
    self = [super initWithContentRect:[screen frame]
                            styleMask:NSBorderlessWindowMask
                              backing:NSBackingStoreBuffered
                                defer:NO];
    if (self == nil)
        return nil;
    _screen = screen;
    _view = [[DesktopView alloc] initWithFrame:NSMakeRect(0, 0, NSWidth([screen frame]), NSHeight([screen frame]))];
    [_view setAutoresizingMask:NSViewWidthSizable | NSViewHeightSizable];
    [self setContentView:_view];
    [self setLevel:kCGDesktopWindowLevelKey];
    [self setBackgroundColor:DesktopDefaultColor()];
    [self setOpaque:YES];
    [self setHasShadow:NO];
    [self setHidesOnDeactivate:NO];
    [self setExcludedFromWindowsMenu:YES];
    [self setReleasedWhenClosed:NO];
    [self setIgnoresMouseEvents:YES];
    [self reload];
    return self;
}

- (void)reload
{
    NSWorkspace *workspace = [NSWorkspace sharedWorkspace];
    NSURL *url = [workspace desktopImageURLForScreen:_screen];
    NSDictionary *options = [workspace desktopImageOptionsForScreen:_screen];
    NSColor *color = options[NSWorkspaceDesktopImageFillColorKey];

    _view.image = url != nil ? [[NSImage alloc] initWithContentsOfURL:url] : nil;
    _view.color = color != nil ? color : DesktopDefaultColor();
    _view.scaling = (NSImageScaling)[options[NSWorkspaceDesktopImageScalingKey] integerValue];
    _view.clipping = [options[NSWorkspaceDesktopImageAllowClippingKey] boolValue];
    [_view setNeedsDisplay:YES];
}

- (BOOL)canBecomeKeyWindow
{
    return NO;
}

- (BOOL)canBecomeMainWindow
{
    return NO;
}

@end
