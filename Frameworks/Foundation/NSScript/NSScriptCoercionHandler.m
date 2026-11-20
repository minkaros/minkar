// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSScriptCoercionHandler.h>
#import <Foundation/Foundation.h>

/* The coercers by pair of classes ("From>To"), each an object and the
   selector it is sent (+coerceValue:toClass: style: value, class). */
@implementation NSScriptCoercionHandler {
    NSMutableDictionary<NSString *, NSArray *> *_coercers;
}

+ (NSScriptCoercionHandler *)sharedCoercionHandler {
    static NSScriptCoercionHandler *shared;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ shared = [self new]; });
    return shared;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _coercers = [NSMutableDictionary dictionary];
    return self;
}

- (void)registerCoercer:(id)coercer selector:(SEL)selector toConvertFromClass:(Class)fromClass toClass:(Class)toClass {
    if (coercer == nil || selector == NULL || fromClass == Nil || toClass == Nil)
        return;
    @synchronized(self) {
        _coercers[[NSString stringWithFormat:@"%@>%@", NSStringFromClass(fromClass), NSStringFromClass(toClass)]] =
            @[ coercer, NSStringFromSelector(selector) ];
    }
}

/* A value of the class already, or converted by the coercer registered
   for its class (or a superclass) and the class asked for; with none, the
   value as it is, as on the reference. */
- (id)coerceValue:(id)value toClass:(Class)toClass {
    if (value == nil || toClass == Nil || [value isKindOfClass:toClass])
        return value;
    NSArray *entry = nil;
    @synchronized(self) {
        for (Class c = [value class]; c != Nil && entry == nil; c = [c superclass])
            entry = _coercers[[NSString stringWithFormat:@"%@>%@", NSStringFromClass(c), NSStringFromClass(toClass)]];
    }
    if (entry == nil)
        return value;
    id coercer = entry[0];
    SEL selector = NSSelectorFromString(entry[1]);
    id (*send)(id, SEL, id, Class) = (id (*)(id, SEL, id, Class))[coercer methodForSelector:selector];
    return send ? send(coercer, selector, value, toClass) : value;
}

@end
