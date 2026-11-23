/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

/* The symbolic hotkeys SystemUIServer owns (the screenshots, the input
 * sources) and the session user's own keys, given to the window server
 * when the session starts. */
@interface SystemHotKeys : NSObject
- (void)start;
@end
