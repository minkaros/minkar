/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * An action that assistive tools offer for an element besides its own
 * (press, increment, ...): a name, and a handler or a target and selector.
 */

#import <Foundation/NSObject.h>

@class NSString;

@interface NSAccessibilityCustomAction : NSObject {
    NSString *_name;
    BOOL (^_handler)(void);
    id _target;
    SEL _selector;
}

- (instancetype)initWithName:(NSString *)name handler:(BOOL (^)(void))handler;
- (instancetype)initWithName:(NSString *)name target:(id)target selector:(SEL)selector;

@property (copy) NSString *name;
@property (copy) BOOL (^handler)(void);
@property (assign) id target;
@property SEL selector;

/* Runs the action; YES if it was done. */
- (BOOL)_perform;

@end
