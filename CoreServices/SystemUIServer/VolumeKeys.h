/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The keyboard's sound keys (the window server's volume hotkeys): the
 * default output's volume up or down a sixteenth (a quarter of that with
 * Shift-Option), mute on and off, as on the reference platform; up or
 * down unmutes. The feedback sound plays as the Sound pane's "Play
 * feedback when volume is changed" says, Shift turning it the other way.
 * Each press shows the level on the screen for a moment.
 */

#import <Cocoa/Cocoa.h>

NS_ASSUME_NONNULL_BEGIN

@interface VolumeKeys : NSObject
/* A volume hotkey (kSLSHotKeyVolumeUp, Down, Mute) with the modifiers. */
- (void)handleHotKey:(uint32_t)hotKey flags:(NSUInteger)flags;
@end

NS_ASSUME_NONNULL_END
