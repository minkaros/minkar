/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "CIFilterRegistry.h"
#import "CIBoxBlur.h"
#import "CILinearGradient.h"

/* Only filters a context draws are listed: an application asking for a
   category finds those Minkar can apply. */
NSArray<Class> *CIBuiltInFilterClasses(void) {
    return @[ [CIBoxBlur class], [CILinearGradient class] ];
}
