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

static inline NSColor *FilesGray(CGFloat white, CGFloat alpha)
{
    if (FilesDark())
        white = white >= 0.6 ? 0.16 + (1 - white) * 0.9 : 1 - white * 0.9;
    return [NSColor colorWithGenericGamma22White:white alpha:alpha];
}

#endif
