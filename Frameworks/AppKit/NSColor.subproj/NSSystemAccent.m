/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The system's accent and highlight colors (System Preferences' General
 * pane), under the reference's keys in the global domain:
 * AppleAccentColor (-1 graphite, 0 red, 1 orange, 2 yellow, 3 green,
 * absent blue, 5 purple, 6 pink) and AppleHighlightColor ("R G B Name",
 * the selected text's background; absent, the accent's own). The System
 * color list's accent colors are set from them when an app starts and
 * again when AppleColorPreferencesChangedNotification comes (a distributed
 * notification, as on the reference); then every window is drawn again
 * and NSSystemColorsDidChangeNotification posted. The tones are Atlas's
 * own palette.
 */

#import <AppKit/NSColor.h>
#import <AppKit/NSColorList.h>
#import <AppKit/NSApplication.h>
#import <AppKit/NSWindow.h>
#import <AppKit/NSView.h>
#import <AppKit/NSMatrix.h>
#import <AppKit/NSButtonCell.h>
#import <Foundation/NSDistributedNotificationCenter.h>
#import <Foundation/NSNotification.h>
#import <CoreFoundation/CoreFoundation.h>

NSString *const _NSColorPreferencesChangedNotification = @"AppleColorPreferencesChangedNotification";

/* An accent: the color itself, a selected row in a focused list (deeper),
   and selected text's background (lighter). */
typedef struct {
    int value;
    CGFloat accent[3], list[3], selection[3];
} NSAccentTones;

static const NSAccentTones kAccents[] = {
    { 4, { 0.13, 0.47, 0.95 }, { 0.10, 0.38, 0.83 }, { 0.70, 0.84, 1.00 } },    /* blue, the default */
    { 5, { 0.58, 0.34, 0.80 }, { 0.48, 0.26, 0.69 }, { 0.88, 0.80, 0.98 } },    /* purple */
    { 6, { 0.93, 0.33, 0.62 }, { 0.80, 0.24, 0.51 }, { 0.99, 0.80, 0.90 } },    /* pink */
    { 0, { 0.91, 0.27, 0.27 }, { 0.78, 0.19, 0.19 }, { 1.00, 0.80, 0.79 } },    /* red */
    { 1, { 0.96, 0.53, 0.13 }, { 0.84, 0.43, 0.07 }, { 1.00, 0.87, 0.72 } },    /* orange */
    { 2, { 0.95, 0.75, 0.10 }, { 0.80, 0.60, 0.03 }, { 1.00, 0.93, 0.68 } },    /* yellow */
    { 3, { 0.28, 0.69, 0.29 }, { 0.20, 0.56, 0.22 }, { 0.80, 0.94, 0.76 } },    /* green */
    { -1, { 0.54, 0.54, 0.57 }, { 0.44, 0.44, 0.47 }, { 0.85, 0.85, 0.87 } },   /* graphite */
};

static NSColor *rgb(const CGFloat c[3], CGFloat alpha) {
    return [NSColor colorWithCalibratedRed:c[0] green:c[1] blue:c[2] alpha:alpha];
}

static id globalValue(CFStringRef key) {
    CFPropertyListRef value = CFPreferencesCopyValue(key, kCFPreferencesAnyApplication,
        kCFPreferencesCurrentUser, kCFPreferencesAnyHost);
    return [(id)value autorelease];
}

/* The accent the global domain names; blue when it names none. */
static const NSAccentTones *currentAccent(void) {
    id value = globalValue(CFSTR("AppleAccentColor"));
    int accent = 4;

    if ([value respondsToSelector:@selector(intValue)])
        accent = [value intValue];
    for (size_t i = 0; i < sizeof(kAccents) / sizeof(kAccents[0]); i++)
        if (kAccents[i].value == accent)
            return &kAccents[i];
    return &kAccents[0];
}

/* "R G B Name": the highlight color chosen, else nil. */
static NSColor *currentHighlight(void) {
    id value = globalValue(CFSTR("AppleHighlightColor"));
    NSArray *parts;

    if (![value isKindOfClass:[NSString class]])
        return nil;
    parts = [value componentsSeparatedByString:@" "];
    if ([parts count] < 3)
        return nil;
    return [NSColor colorWithCalibratedRed:[[parts objectAtIndex:0] doubleValue]
                                     green:[[parts objectAtIndex:1] doubleValue]
                                      blue:[[parts objectAtIndex:2] doubleValue] alpha:1];
}

void _NSApplySystemAccentColors(void) {
    NSColorList *list = [NSColorList colorListNamed:@"System"];
    const NSAccentTones *tones = currentAccent();
    NSColor *accent = rgb(tones->accent, 1), *rows = rgb(tones->list, 1);
    NSColor *selection = currentHighlight() ?: rgb(tones->selection, 1);

    if (list == nil)
        return;
    [list setColor:accent forKey:@"controlAccentColor"];
    [list setColor:accent forKey:@"selectedMenuItemColor"];
    [list setColor:accent forKey:@"textInsertionPointColor"];
    [list setColor:rgb(tones->accent, 0.5) forKey:@"keyboardFocusIndicatorColor"];
    [list setColor:rows forKey:@"selectedContentBackgroundColor"];
    [list setColor:rows forKey:@"alternateSelectedControlColor"];
    [list setColor:selection forKey:@"selectedTextBackgroundColor"];
    [list setColor:selection forKey:@"selectedControlColor"];
}

NSArray *_NSGraphicsStyleAccentChanged(void);

/* A button's cells holding an old check box or radio button image get the
   new one; then its subviews. */
static void swapToggleImages(NSView *view, NSArray *pairs) {
    NSArray *cells = nil;

    if ([view isKindOfClass:[NSMatrix class]])
        cells = [(NSMatrix *)view cells];
    else if ([view isKindOfClass:[NSControl class]] && [[(NSControl *)view cell] isKindOfClass:[NSButtonCell class]])
        cells = [NSArray arrayWithObject:[(NSControl *)view cell]];
    for (id cell in cells) {
        if (![cell isKindOfClass:[NSButtonCell class]])
            continue;
        for (NSUInteger i = 0; i + 1 < [pairs count]; i += 2) {
            if ([cell image] == [pairs objectAtIndex:i])
                [cell setImage:[pairs objectAtIndex:i + 1]];
            if ([cell alternateImage] == [pairs objectAtIndex:i])
                [cell setAlternateImage:[pairs objectAtIndex:i + 1]];
        }
    }
    for (NSView *sub in [view subviews])
        swapToggleImages(sub, pairs);
}

@interface _NSSystemAccentWatcher : NSObject
@end

@implementation _NSSystemAccentWatcher

/* The pane changed them: read again, then every window drawn anew. */
- (void)colorsChanged:(NSNotification *)note {
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    _NSApplySystemAccentColors();
    NSArray *pairs = _NSGraphicsStyleAccentChanged();
    for (NSWindow *window in [NSApp windows]) {
        NSView *top = [[window contentView] superview];
        swapToggleImages(top != nil ? top : [window contentView], pairs);
        if ([window isVisible])
            [window display];
    }
    [[NSNotificationCenter defaultCenter] postNotificationName:NSSystemColorsDidChangeNotification object:nil];
}

@end

void _NSWatchSystemAccentColors(void) {
    static _NSSystemAccentWatcher *watcher;

    if (watcher != nil)
        return;
    watcher = [_NSSystemAccentWatcher new];
    _NSApplySystemAccentColors();
    [[NSDistributedNotificationCenter defaultCenter] addObserver:watcher selector:@selector(colorsChanged:)
        name:_NSColorPreferencesChangedNotification object:nil];
}
