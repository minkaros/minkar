// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/Foundation.h>
#import <Foundation/NSScriptKeyValueCoding.h>
#import <Foundation/NSScriptClassDescription.h>
#import <Foundation/NSScriptCoercionHandler.h>

NSString *const NSOperationNotSupportedForKeyException = @"NSOperationNotSupportedForKeyException";

/* "valueIn<Key>AtIndex:" and the like for a key. */
static SEL keyedSelector(NSString *format, NSString *key) {
    NSString *capitalized = [[[key substringToIndex:1] uppercaseString] stringByAppendingString:[key substringFromIndex:1]];
    return NSSelectorFromString([NSString stringWithFormat:format, capitalized]);
}

@implementation NSObject (NSScriptKeyValueCoding)

- (id)valueAtIndex:(NSUInteger)index inPropertyWithKey:(NSString *)key {
    SEL selector = keyedSelector(@"valueIn%@AtIndex:", key);

    if ([self respondsToSelector:selector])
        return ((id (*)(id, SEL, NSUInteger))[self methodForSelector:selector])(self, selector, index);
    NSArray *elements = [self valueForKey:key];
    return (index < [elements count]) ? [elements objectAtIndex:index] : nil;
}

- (id)valueWithName:(NSString *)name inPropertyWithKey:(NSString *)key {
    SEL selector = keyedSelector(@"valueIn%@WithName:", key);

    if ([self respondsToSelector:selector])
        return [self performSelector:selector withObject:name];
    for (id element in [self valueForKey:key])
        if ([element respondsToSelector:@selector(name)] && [[element valueForKey:@"name"] isEqual:name])
            return element;
    return nil;
}

- (id)valueWithUniqueID:(id)uniqueID inPropertyWithKey:(NSString *)key {
    SEL selector = keyedSelector(@"valueIn%@WithUniqueID:", key);

    if ([self respondsToSelector:selector])
        return [self performSelector:selector withObject:uniqueID];
    for (id element in [self valueForKey:key])
        if ([element respondsToSelector:@selector(uniqueID)] && [[element valueForKey:@"uniqueID"] isEqual:uniqueID])
            return element;
    return nil;
}

- (void)insertValue:(id)value atIndex:(NSUInteger)index inPropertyWithKey:(NSString *)key {
    SEL selector = keyedSelector(@"insertIn%@:atIndex:", key);

    if ([self respondsToSelector:selector]) {
        ((void (*)(id, SEL, id, NSUInteger))[self methodForSelector:selector])(self, selector, value, index);
        return;
    }
    [[self mutableArrayValueForKey:key] insertObject:value atIndex:index];
}

- (void)removeValueAtIndex:(NSUInteger)index fromPropertyWithKey:(NSString *)key {
    SEL selector = keyedSelector(@"removeFrom%@AtIndex:", key);

    if ([self respondsToSelector:selector]) {
        ((void (*)(id, SEL, NSUInteger))[self methodForSelector:selector])(self, selector, index);
        return;
    }
    [[self mutableArrayValueForKey:key] removeObjectAtIndex:index];
}

- (void)replaceValueAtIndex:(NSUInteger)index inPropertyWithKey:(NSString *)key withValue:(id)value {
    SEL selector = keyedSelector(@"replaceIn%@:atIndex:", key);

    if ([self respondsToSelector:selector]) {
        ((void (*)(id, SEL, id, NSUInteger))[self methodForSelector:selector])(self, selector, value, index);
        return;
    }
    [[self mutableArrayValueForKey:key] replaceObjectAtIndex:index withObject:value];
}

/* At the end, or by insertIn<Key>: when there is one. */
- (void)insertValue:(id)value inPropertyWithKey:(NSString *)key {
    SEL selector = keyedSelector(@"insertIn%@:", key);

    if ([self respondsToSelector:selector]) {
        [self performSelector:selector withObject:value];
        return;
    }
    NSMutableArray *elements = [self mutableArrayValueForKey:key];
    [elements insertObject:value atIndex:[elements count]];
}

/* By coerceValueFor<Key>: when there is one, otherwise to the class of
   the key's type in the scripting class description. */
- (id)coerceValue:(id)value forKey:(NSString *)key {
    SEL selector = keyedSelector(@"coerceValueFor%@:", key);
    NSScriptClassDescription *description;
    NSString *type;
    Class kind;

    if ([self respondsToSelector:selector])
        return [self performSelector:selector withObject:value];
    description = [NSScriptClassDescription classDescriptionForClass:[self class]];
    type = [description typeForKey:key];
    kind = (type != nil) ? NSClassFromString(type) : Nil;
    if (value == nil || kind == Nil || [value isKindOfClass:kind])
        return value;
    return [[NSScriptCoercionHandler sharedCoercionHandler] coerceValue:value toClass:kind];
}

@end
