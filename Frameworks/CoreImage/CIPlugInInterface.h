/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/Foundation.h>

/* What an image unit's principal class answers as it is loaded: YES when
   it is ready (it may register filters then). */
@protocol CIPlugInRegistration
- (BOOL)load:(void *)host;
@end
