/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Custom content (AXCustomContent.h) and settings (AXSettings.h).
 */

#import <Accessibility/Accessibility.h>

NSNotificationName const AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification =
    @"AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification";

static NSString *const kNonBlinkingKey = @"AXPrefersNonBlinkingTextInsertionIndicator";

/* The distributed notification of a settings change, passed on to the
   app's own center, as the API's clients observe it there. */
static void forward(CFNotificationCenterRef center, void *observer, CFNotificationName name, const void *object,
                    CFDictionaryRef info) {
    dispatch_async(dispatch_get_main_queue(), ^{
        [[NSNotificationCenter defaultCenter]
            postNotificationName:AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification object:nil];
    });
}

BOOL AXPrefersNonBlinkingTextInsertionIndicator(void) {
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        CFNotificationCenterAddObserver(CFNotificationCenterGetDistributedCenter(), NULL, forward,
            (__bridge CFStringRef)AXPrefersNonBlinkingTextInsertionIndicatorDidChangeNotification, NULL,
            CFNotificationSuspensionBehaviorDeliverImmediately);
    });
    Boolean valid = false;
    CFPreferencesAppSynchronize(kCFPreferencesAnyApplication);
    Boolean on = CFPreferencesGetAppBooleanValue((__bridge CFStringRef)kNonBlinkingKey, kCFPreferencesAnyApplication, &valid);
    return valid && on;
}

@interface AXCustomContent ()
- (instancetype)_init;
@end

@implementation AXCustomContent {
    NSAttributedString *_attributedLabel;
    NSAttributedString *_attributedValue;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (instancetype)customContentWithLabel:(NSString *)label value:(NSString *)value {
    return [self customContentWithAttributedLabel:[[NSAttributedString alloc] initWithString:label ?: @""]
                                  attributedValue:[[NSAttributedString alloc] initWithString:value ?: @""]];
}

+ (instancetype)customContentWithAttributedLabel:(NSAttributedString *)label attributedValue:(NSAttributedString *)value {
    AXCustomContent *content = [[self alloc] _init];
    content->_attributedLabel = [label copy] ?: [[NSAttributedString alloc] initWithString:@""];
    content->_attributedValue = [value copy] ?: [[NSAttributedString alloc] initWithString:@""];
    return content;
}

- (instancetype)_init {
    return [super init];
}

- (NSString *)label { return [_attributedLabel string]; }
- (NSAttributedString *)attributedLabel { return _attributedLabel; }
- (NSString *)value { return [_attributedValue string]; }
- (NSAttributedString *)attributedValue { return _attributedValue; }

- (id)copyWithZone:(NSZone *)zone {
    AXCustomContent *copy = [AXCustomContent customContentWithAttributedLabel:_attributedLabel attributedValue:_attributedValue];
    copy.importance = self.importance;
    return copy;
}

- (BOOL)isEqual:(id)other {
    if (![other isKindOfClass:[AXCustomContent class]])
        return NO;
    AXCustomContent *o = other;
    return [o->_attributedLabel isEqual:_attributedLabel] && [o->_attributedValue isEqual:_attributedValue] &&
           o.importance == self.importance;
}

- (NSUInteger)hash {
    return [_attributedLabel hash] ^ [_attributedValue hash];
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_attributedLabel forKey:@"label"];
    [coder encodeObject:_attributedValue forKey:@"value"];
    [coder encodeInteger:(NSInteger)self.importance forKey:@"importance"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) != nil) {
        _attributedLabel = [[coder decodeObjectOfClass:[NSAttributedString class] forKey:@"label"] copy];
        _attributedValue = [[coder decodeObjectOfClass:[NSAttributedString class] forKey:@"value"] copy];
        self.importance = (AXCustomContentImportance)[coder decodeIntegerForKey:@"importance"];
    }
    return self;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<AXCustomContent %@: %@>", self.label, self.value];
}

@end
