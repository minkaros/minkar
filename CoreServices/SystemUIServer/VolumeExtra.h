/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "MenuExtra.h"

NS_ASSUME_NONNULL_BEGIN

/* The volume: a speaker in the menu bar, as loud as the default output;
   a click opens a panel below it with the volume, mute, and the Sound
   preferences. Through CoreAudio, its listeners keeping it current. */
@interface VolumeExtra : NSObject <MenuExtra>
@end

NS_ASSUME_NONNULL_END
