/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A nib's user defined runtime attributes: values Interface Builder sets on
 * an object by key path once the nib's objects are connected.
 */

#import <AppKit/NSNibConnector.h>

@interface NSIBUserDefinedRuntimeAttributesConnector : NSNibConnector {
    id _object;
    NSArray *_keyPaths;
    NSArray *_values;
}
@end
