/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <AppKit/NSAccessibilityCustomAction.h>
#import <Foundation/NSString.h>
#import <objc/message.h>

@implementation NSAccessibilityCustomAction

- (instancetype)initWithName:(NSString *)name handler:(BOOL (^)(void))handler {
    if ((self = [super init]) != nil) {
        _name = [name copy];
        _handler = [handler copy];
    }
    return self;
}

- (instancetype)initWithName:(NSString *)name target:(id)target selector:(SEL)selector {
    if ((self = [super init]) != nil) {
        _name = [name copy];
        _target = target;
        _selector = selector;
    }
    return self;
}

- (void)dealloc {
    [_name release];
    [_handler release];
    [super dealloc];
}

- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { name = [name copy]; [_name release]; _name = name; }
- (BOOL (^)(void))handler { return _handler; }
- (void)setHandler:(BOOL (^)(void))handler { handler = [handler copy]; [_handler release]; _handler = handler; }
- (id)target { return _target; }
- (void)setTarget:(id)target { _target = target; }
- (SEL)selector { return _selector; }
- (void)setSelector:(SEL)selector { _selector = selector; }

- (BOOL)_perform {
    if (_handler != nil)
        return _handler();
    if (_target != nil && _selector != NULL && [_target respondsToSelector:_selector])
        return ((BOOL (*)(id, SEL, id))objc_msgSend)(_target, _selector, self);
    return NO;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p %@>", [self class], self, _name];
}

@end
