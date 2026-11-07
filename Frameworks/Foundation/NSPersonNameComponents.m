/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A person's name in parts, and its formatter (API 19): given name first,
 * as the Western order the locales Minkar ships with use. ARC.
 */

#import <Foundation/NSPersonNameComponents.h>
#import <Foundation/NSPersonNameComponentsFormatter.h>
#import <Foundation/NSString.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSMutableArray.h>
#import <Foundation/NSCharacterSet.h>
#import <Foundation/NSCoder.h>
#import <Foundation/NSAttributedString.h>
#import <Foundation/NSMutableAttributedString.h>
#import <Foundation/NSDictionary.h>

NSString * const NSPersonNameComponentKey = @"NSPersonNameComponentKey";
NSString * const NSPersonNameComponentGivenName = @"givenName";
NSString * const NSPersonNameComponentFamilyName = @"familyName";
NSString * const NSPersonNameComponentMiddleName = @"middleName";
NSString * const NSPersonNameComponentPrefix = @"namePrefix";
NSString * const NSPersonNameComponentSuffix = @"nameSuffix";
NSString * const NSPersonNameComponentNickname = @"nickname";
NSString * const NSPersonNameComponentDelimiter = @"delimiter";

static BOOL same(NSString *a, NSString *b) {
    return a == b || [a isEqualToString:b];
}

@implementation NSPersonNameComponents

+ (BOOL)supportsSecureCoding {
    return YES;
}

- (id)copyWithZone:(NSZone *)zone {
    NSPersonNameComponents *copy = [[NSPersonNameComponents allocWithZone:zone] init];
    copy.namePrefix = self.namePrefix;
    copy.givenName = self.givenName;
    copy.middleName = self.middleName;
    copy.familyName = self.familyName;
    copy.nameSuffix = self.nameSuffix;
    copy.nickname = self.nickname;
    copy.phoneticRepresentation = self.phoneticRepresentation;
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:self.namePrefix forKey:@"NS.namePrefix"];
    [coder encodeObject:self.givenName forKey:@"NS.givenName"];
    [coder encodeObject:self.middleName forKey:@"NS.middleName"];
    [coder encodeObject:self.familyName forKey:@"NS.familyName"];
    [coder encodeObject:self.nameSuffix forKey:@"NS.nameSuffix"];
    [coder encodeObject:self.nickname forKey:@"NS.nickname"];
    [coder encodeObject:self.phoneticRepresentation forKey:@"NS.phoneticRepresentation"];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) == nil)
        return nil;
    Class s = [NSString class];
    self.namePrefix = [coder decodeObjectOfClass:s forKey:@"NS.namePrefix"];
    self.givenName = [coder decodeObjectOfClass:s forKey:@"NS.givenName"];
    self.middleName = [coder decodeObjectOfClass:s forKey:@"NS.middleName"];
    self.familyName = [coder decodeObjectOfClass:s forKey:@"NS.familyName"];
    self.nameSuffix = [coder decodeObjectOfClass:s forKey:@"NS.nameSuffix"];
    self.nickname = [coder decodeObjectOfClass:s forKey:@"NS.nickname"];
    self.phoneticRepresentation = [coder decodeObjectOfClass:[NSPersonNameComponents class] forKey:@"NS.phoneticRepresentation"];
    return self;
}

- (BOOL)isEqual:(id)other {
    if (other == self)
        return YES;
    if (![other isKindOfClass:[NSPersonNameComponents class]])
        return NO;
    NSPersonNameComponents *o = other;
    return same(self.namePrefix, o.namePrefix) && same(self.givenName, o.givenName) &&
        same(self.middleName, o.middleName) && same(self.familyName, o.familyName) &&
        same(self.nameSuffix, o.nameSuffix) && same(self.nickname, o.nickname) &&
        (self.phoneticRepresentation == o.phoneticRepresentation || [self.phoneticRepresentation isEqual:o.phoneticRepresentation]);
}

- (NSUInteger)hash {
    return [self.givenName hash] ^ [self.familyName hash];
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p> %@", [self class], (void *)self,
        [NSPersonNameComponentsFormatter localizedStringFromPersonNameComponents:self
            style:NSPersonNameComponentsFormatterStyleLong options:0]];
}

@end

@implementation NSPersonNameComponentsFormatter

+ (NSString *)localizedStringFromPersonNameComponents:(NSPersonNameComponents *)components style:(NSPersonNameComponentsFormatterStyle)nameFormatStyle options:(NSPersonNameComponentsFormatterOptions)nameOptions {
    NSPersonNameComponentsFormatter *formatter = [[self alloc] init];
    formatter.style = nameFormatStyle;
    formatter.phonetic = (nameOptions & NSPersonNameComponentsFormatterPhonetic) != 0;
    return [formatter stringFromPersonNameComponents:components];
}

- (NSPersonNameComponentsFormatterStyle)style {
    return _style;
}

- (void)setStyle:(NSPersonNameComponentsFormatterStyle)style {
    _style = style;
}

- (BOOL)isPhonetic {
    return _phonetic;
}

- (void)setPhonetic:(BOOL)phonetic {
    _phonetic = phonetic;
}

/* The parts the style shows, each with its key: short is the given name
 * (else the nickname), medium given and family, long everything,
 * abbreviated the initials. */
- (NSArray *)_partsOf:(NSPersonNameComponents *)c {
    if (_phonetic && c.phoneticRepresentation != nil)
        c = c.phoneticRepresentation;
    NSMutableArray *parts = [NSMutableArray array];
    void (^add)(NSString *, NSString *) = ^(NSString *value, NSString *key) {
        if ([value length] > 0)
            [parts addObject:@[value, key]];
    };
    switch (_style) {
    case NSPersonNameComponentsFormatterStyleShort:
        add(c.nickname ?: c.givenName ?: c.familyName, c.nickname ? NSPersonNameComponentNickname :
            c.givenName ? NSPersonNameComponentGivenName : NSPersonNameComponentFamilyName);
        break;
    case NSPersonNameComponentsFormatterStyleLong:
        add(c.namePrefix, NSPersonNameComponentPrefix);
        add(c.givenName, NSPersonNameComponentGivenName);
        add(c.middleName, NSPersonNameComponentMiddleName);
        add(c.familyName, NSPersonNameComponentFamilyName);
        add(c.nameSuffix, NSPersonNameComponentSuffix);
        break;
    case NSPersonNameComponentsFormatterStyleAbbreviated: {
        NSMutableString *initials = [NSMutableString string];
        for (NSString *name in @[c.givenName ?: @"", c.familyName ?: @""])
            if ([name length] > 0)
                [initials appendString:[[name substringToIndex:[name rangeOfComposedCharacterSequenceAtIndex:0].length] uppercaseString]];
        add(initials, NSPersonNameComponentGivenName);
        return parts;
    }
    default:
        add(c.givenName, NSPersonNameComponentGivenName);
        add(c.familyName, NSPersonNameComponentFamilyName);
        if ([parts count] == 0)
            add(c.nickname, NSPersonNameComponentNickname);
        break;
    }
    return parts;
}

- (NSString *)stringFromPersonNameComponents:(NSPersonNameComponents *)components {
    NSMutableArray *words = [NSMutableArray array];
    for (NSArray *part in [self _partsOf:components])
        [words addObject:part[0]];
    return [words componentsJoinedByString:@" "];
}

- (NSAttributedString *)annotatedStringFromPersonNameComponents:(NSPersonNameComponents *)components {
    NSMutableAttributedString *result = [[NSMutableAttributedString alloc] init];
    BOOL first = YES;
    for (NSArray *part in [self _partsOf:components]) {
        if (!first)
            [result appendAttributedString:[[NSAttributedString alloc] initWithString:@" "
                attributes:@{ NSPersonNameComponentKey: NSPersonNameComponentDelimiter }]];
        [result appendAttributedString:[[NSAttributedString alloc] initWithString:part[0]
            attributes:@{ NSPersonNameComponentKey: part[1] }]];
        first = NO;
    }
    return result;
}

/* Words: the first given, the last family, those between middle names. */
- (NSPersonNameComponents *)personNameComponentsFromString:(NSString *)string {
    NSMutableArray *words = [NSMutableArray array];
    for (NSString *word in [string componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceAndNewlineCharacterSet]])
        if ([word length] > 0)
            [words addObject:word];
    if ([words count] == 0)
        return nil;
    NSPersonNameComponents *c = [[NSPersonNameComponents alloc] init];
    c.givenName = words[0];
    if ([words count] > 1)
        c.familyName = [words lastObject];
    if ([words count] > 2)
        c.middleName = [[words subarrayWithRange:NSMakeRange(1, [words count] - 2)] componentsJoinedByString:@" "];
    return c;
}

- (NSString *)stringForObjectValue:(id)object {
    return [object isKindOfClass:[NSPersonNameComponents class]] ? [self stringFromPersonNameComponents:object] : nil;
}

- (BOOL)getObjectValue:(id *)obj forString:(NSString *)string errorDescription:(NSString **)error {
    NSPersonNameComponents *c = [self personNameComponentsFromString:string];
    if (obj != NULL)
        *obj = c;
    if (c == nil && error != NULL)
        *error = @"Not a name";
    return c != nil;
}

@end
