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

static inline NSColor *FilesGray(CGFloat white, CGFloat alpha)
{
    if (FilesDark())
        white = FilesDarkGray(white);
    return [NSColor colorWithGenericGamma22White:white alpha:alpha];
}

#endif
