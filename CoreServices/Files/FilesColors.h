/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Files' greys in both appearances (General in System Preferences): in
 * the light one the grey asked for; in the dark one its counterpart,
 * backgrounds and lines (light greys) dark, text (dark greys) light.
 */

#ifndef FILESCOLORS_H
#define FILESCOLORS_H

#import <Cocoa/Cocoa.h>

static inline BOOL FilesDark(void)
{
    return [[[NSApp effectiveAppearance] name] isEqualToString:NSAppearanceNameDarkAqua];
}

/* The dark counterparts, as the maintainer chose them (23 Nov 2026): the
   content 0.20, headers and alternate rows 0.215, the sidebar 0.17, a
   chosen row or segment 0.13 (darker than its list), lines 0.09; text
   light, 0.93 for black. */
static inline CGFloat FilesDarkGray(CGFloat white)
{
    if (white < 0.6)
        return 0.93 - white * 0.6;
    if (white >= 0.99)
        return 0.20;
    if (white >= 0.94)
        return 0.215;
    if (white >= 0.90)
        return 0.17;
    if (white >= 0.84)
        return 0.09;
    if (white >= 0.78)
        return 0.13;
    return 0.30;
}

/* With an appearance theme (General's Themes), each grey is the theme's
   color of its part instead, so that the theme reaches Files. Only the
   colors every theme gives are used (the window's and the content's
   backgrounds, text, secondary text, lines); the others are mixes of
   them, so that they suit the theme: headers and alternate rows the
   content a little toward the text, a chosen row the sidebar more so. */
extern BOOL _NSAppearanceThemeActive(void);

static inline NSColor *FilesThemeColor(CGFloat white)
{
    if (white < 0.3)
        return [NSColor labelColor];
    if (white < 0.6)
        return [NSColor secondaryLabelColor];
    if (white >= 0.99)
        return [NSColor controlBackgroundColor];
    if (white >= 0.94)
        return [[NSColor controlBackgroundColor] blendedColorWithFraction:0.06 ofColor:[NSColor labelColor]];
    if (white >= 0.90)
        return [NSColor windowBackgroundColor];
    if (white >= 0.84)
        return [NSColor separatorColor];
    if (white >= 0.78)
        return [[NSColor windowBackgroundColor] blendedColorWithFraction:0.18 ofColor:[NSColor labelColor]];
    return [[NSColor windowBackgroundColor] blendedColorWithFraction:0.30 ofColor:[NSColor labelColor]];
}

/* A control's grey (the bar's buttons): with an appearance theme, AppKit's
   for its controls (the theme's face, track and border); otherwise
   Files's own. */
extern NSColor *_NSControlGray(CGFloat white);
extern NSColor *_NSAppearanceThemeColor(NSString *key);
static inline NSColor *FilesGray(CGFloat white, CGFloat alpha);
static inline NSColor *FilesControlGray(CGFloat white)
{
    if (_NSAppearanceThemeActive())
        return _NSControlGray(white);
    return FilesGray(white, 1);
}

static inline NSColor *FilesGray(CGFloat white, CGFloat alpha)
{
    if (_NSAppearanceThemeActive())
        return [FilesThemeColor(white) colorWithAlphaComponent:alpha];
    if (FilesDark())
        white = FilesDarkGray(white);
    return [NSColor colorWithGenericGamma22White:white alpha:alpha];
}

#endif
