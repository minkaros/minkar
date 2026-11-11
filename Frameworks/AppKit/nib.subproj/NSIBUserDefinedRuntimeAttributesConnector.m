/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "NSIBUserDefinedRuntimeAttributesConnector.h"
#import <Foundation/NSArray.h>
#import <Foundation/NSKeyedArchiver.h>
#import <Foundation/NSKeyValueCoding.h>
#import <Foundation/NSException.h>

@implementation NSIBUserDefinedRuntimeAttributesConnector

- initWithCoder:(NSCoder *)coder {
    _object = [[coder decodeObjectForKey:@"NSObject"] retain];
    _keyPaths = [[coder decodeObjectForKey:@"NSKeyPaths"] retain];
    _values = [[coder decodeObjectForKey:@"NSValues"] retain];
    return self;
}

- (void)dealloc {
    [_object release];
    [_keyPaths release];
    [_values release];
    [super dealloc];
}

- (void)replaceObject:original withObject:replacement {
    if (original == _object) {
        [replacement retain];
        [_object release];
        _object = replacement;
    }
    [super replaceObject:original withObject:replacement];
}

/* Each value set in order; a key path the object does not have is
   reported, as on the reference platform, and the others still set. */
- (void)establishConnection {
    NSUInteger count = MIN([_keyPaths count], [_values count]);

    for (NSUInteger i = 0; i < count; i++) {
        NSString *keyPath = [_keyPaths objectAtIndex:i];
        @try {
            [_object setValue:[_values objectAtIndex:i] forKeyPath:keyPath];
        } @catch (NSException *exception) {
            NSLog(@"Failed to set (%@) user defined inspected property on (%@): %@", keyPath, [_object class], [exception reason]);
        }
    }
}

@end
