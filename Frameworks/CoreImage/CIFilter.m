/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <CoreImage/CIFilter.h>
#import <CoreImage/CIFilterConstructor.h>
#import <CoreImage/CIImage.h>
#import "CIFilterRegistry.h"
#import <objc/runtime.h>

/* The filters by name: each a class (the built-in ones, made by
   +[CIFilter filterWithName:] with alloc/init) or a constructor (those
   registered), with the attributes given when it was registered. */
@interface _CIFilterEntry : NSObject
@property (nonatomic) Class filterClass;
@property (nonatomic) id<CIFilterConstructor> constructor;
@property (nonatomic, copy) NSDictionary<NSString *, id> *attributes;
@end

@implementation _CIFilterEntry
@end

static NSMutableDictionary<NSString *, _CIFilterEntry *> *registry(void) {
    static NSMutableDictionary *entries;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        entries = [NSMutableDictionary dictionary];
        for (Class cls in CIBuiltInFilterClasses()) {
            _CIFilterEntry *entry = [_CIFilterEntry new];
            entry.filterClass = cls;
            entry.attributes = [cls customAttributes] ?: @{};
            entries[NSStringFromClass(cls)] = entry;
        }
    });
    return entries;
}

static NSLock *registryLock(void) {
    static NSLock *lock;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ lock = [NSLock new]; });
    return lock;
}

/* "inputBackgroundImage" -> "Background Image", "inputPoint0" -> "Point 0". */
static NSString *displayNameForKey(NSString *key) {
    NSString *base = [key hasPrefix:@"input"] ? [key substringFromIndex:5] : [key hasPrefix:@"output"] ? [key substringFromIndex:6] : key;
    NSMutableString *result = [NSMutableString string];
    for (NSUInteger i = 0; i < base.length; i++) {
        unichar c = [base characterAtIndex:i];
        if (i > 0) {
            unichar previous = [base characterAtIndex:i - 1];
            BOOL upper = [[NSCharacterSet uppercaseLetterCharacterSet] characterIsMember:c];
            BOOL digit = [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:c];
            BOOL previousDigit = [[NSCharacterSet decimalDigitCharacterSet] characterIsMember:previous];
            if (upper || (digit && !previousDigit))
                [result appendString:@" "];
        }
        [result appendFormat:@"%C", c];
    }
    return result;
}

/* The class a property holds (T@"NSNumber"), NSObject for an id. */
static NSString *classNameOfProperty(objc_property_t property) {
    const char *attributes = property_getAttributes(property);
    const char *start = attributes ? strstr(attributes, "T@\"") : NULL;
    if (start == NULL)
        return @"NSObject";
    start += 3;
    const char *end = strchr(start, '"');
    if (end == NULL || end == start)
        return @"NSObject";
    NSString *name = [[NSString alloc] initWithBytes:start length:(NSUInteger)(end - start) encoding:NSUTF8StringEncoding];
    NSRange protocol = [name rangeOfString:@"<"];
    if (protocol.location == 0)
        return @"NSObject";
    return protocol.location != NSNotFound ? [name substringToIndex:protocol.location] : name;
}

/* A class's own keys with a prefix: its properties, then its instance
   variables (with or without an underscore), in declaration order, the
   superclasses' first, up to CIFilter. */
static NSArray<NSString *> *declaredKeys(Class cls, NSString *prefix, NSMutableDictionary<NSString *, NSString *> *classNames) {
    NSMutableArray<NSString *> *keys = [NSMutableArray array];
    NSMutableArray<Class> *chain = [NSMutableArray array];
    for (Class c = cls; c != Nil && c != [CIFilter class]; c = class_getSuperclass(c))
        [chain insertObject:c atIndex:0];
    for (Class c in chain) {
        unsigned count = 0;
        objc_property_t *properties = class_copyPropertyList(c, &count);
        for (unsigned i = 0; i < count; i++) {
            NSString *name = @(property_getName(properties[i]));
            if ([name hasPrefix:prefix] && ![keys containsObject:name]) {
                [keys addObject:name];
                classNames[name] = classNameOfProperty(properties[i]);
            }
        }
        free(properties);
        Ivar *ivars = class_copyIvarList(c, &count);
        for (unsigned i = 0; i < count; i++) {
            const char *raw = ivar_getName(ivars[i]);
            if (raw == NULL)
                continue;
            NSString *name = @(raw[0] == '_' ? raw + 1 : raw);
            if ([name hasPrefix:prefix] && ![keys containsObject:name]) {
                [keys addObject:name];
                const char *type = ivar_getTypeEncoding(ivars[i]);
                NSString *className = @"NSObject";
                if (type && type[0] == '@' && type[1] == '"') {
                    const char *end = strchr(type + 2, '"');
                    if (end && end > type + 2)
                        className = [[NSString alloc] initWithBytes:type + 2 length:(NSUInteger)(end - type - 2) encoding:NSUTF8StringEncoding];
                }
                classNames[name] = className;
            }
        }
        free(ivars);
    }
    return keys;
}

@implementation CIFilter {
    NSString *_name;
    BOOL _enabled;
}

+ (BOOL)supportsSecureCoding {
    return YES;
}

+ (NSDictionary<NSString *, id> *)customAttributes {
    return nil;
}

- (instancetype)init {
    if ((self = [super init]) != nil)
        _enabled = YES;
    return self;
}

#pragma mark Name and state

- (NSString *)name {
    return _name ?: NSStringFromClass([self class]);
}

- (void)setName:(NSString *)name {
    _name = [name copy];
}

- (BOOL)isEnabled {
    return _enabled;
}

- (void)setEnabled:(BOOL)enabled {
    _enabled = enabled;
}

#pragma mark Keys and attributes

/* The plain class has no inputs (nil, as on the reference). */
- (NSArray<NSString *> *)inputKeys {
    if ([self class] == [CIFilter class])
        return nil;
    NSMutableArray<NSString *> *keys = [declaredKeys([self class], @"input", [NSMutableDictionary dictionary]) mutableCopy];
    for (NSString *key in [[self class] customAttributes])
        if ([key hasPrefix:@"input"] && ![keys containsObject:key])
            [keys addObject:key];
    return keys;
}

- (NSArray<NSString *> *)outputKeys {
    if ([self class] == [CIFilter class])
        return nil;
    NSMutableArray<NSString *> *keys = [declaredKeys([self class], @"output", [NSMutableDictionary dictionary]) mutableCopy];
    BOOL ownOutputImage = class_getMethodImplementation([self class], @selector(outputImage)) !=
                          class_getMethodImplementation([CIFilter class], @selector(outputImage));
    if (ownOutputImage && ![keys containsObject:@"outputImage"])
        [keys insertObject:@"outputImage" atIndex:0];
    return keys;
}

/* What each input takes (its class, a name to show, its type), from its
   declaration, merged with the class's customAttributes, and the filter's
   name. The plain class has none. */
- (NSDictionary<NSString *, id> *)attributes {
    Class cls = [self class];
    if (cls == [CIFilter class])
        return @{};

    NSMutableDictionary<NSString *, NSString *> *classNames = [NSMutableDictionary dictionary];
    NSArray<NSString *> *inputs = declaredKeys(cls, @"input", classNames);
    NSMutableDictionary<NSString *, id> *result = [NSMutableDictionary dictionary];
    for (NSString *key in inputs) {
        NSMutableDictionary *attribute = [NSMutableDictionary dictionary];
        attribute[kCIAttributeClass] = classNames[key] ?: @"NSObject";
        attribute[kCIAttributeDisplayName] = displayNameForKey(key);
        if ([key hasSuffix:@"Image"])
            attribute[kCIAttributeType] = kCIAttributeTypeImage;
        result[key] = attribute;
    }

    NSDictionary<NSString *, id> *custom = [cls customAttributes];
    for (NSString *key in custom) {
        id value = custom[key];
        if ([value isKindOfClass:[NSDictionary class]] && [result[key] isKindOfClass:[NSDictionary class]]) {
            NSMutableDictionary *merged = [result[key] mutableCopy];
            [merged addEntriesFromDictionary:value];
            result[key] = merged;
        } else
            result[key] = value;
    }

    NSString *name = self.name;
    result[kCIAttributeFilterName] = name;
    if (result[kCIAttributeFilterDisplayName] == nil)
        result[kCIAttributeFilterDisplayName] = name;
    return result;
}

/* Each input with a default in its attributes is set to it. */
- (void)setDefaults {
    NSDictionary<NSString *, id> *attributes = self.attributes;
    for (NSString *key in self.inputKeys) {
        NSDictionary *attribute = attributes[key];
        if (![attribute isKindOfClass:[NSDictionary class]])
            continue;
        id value = attribute[kCIAttributeDefault];
        if (value != nil)
            [self setValue:value forKey:key];
    }
}

#pragma mark Output

/* An image the filter's output stands for: a context draws what the
   filter makes. Nil when the filter has no output of its own. */
- (CIImage *)outputImage {
    return nil;
}

#pragma mark Copying and coding

- (id)copyWithZone:(NSZone *)zone {
    CIFilter *copy = [[[self class] allocWithZone:zone] init];
    copy->_name = [_name copy];
    copy->_enabled = _enabled;
    for (NSString *key in self.inputKeys) {
        id value = [self valueForKey:key];
        if (value != nil)
            [copy setValue:value forKey:key];
    }
    return copy;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_name forKey:@"CIName"];
    [coder encodeBool:_enabled forKey:@"CIEnabled"];
    for (NSString *key in self.inputKeys) {
        id value = [self valueForKey:key];
        if ([value conformsToProtocol:@protocol(NSCoding)])
            [coder encodeObject:value forKey:[@"CI_" stringByAppendingString:key]];
    }
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [self init]) == nil)
        return nil;
    if ([coder containsValueForKey:@"CIName"])
        _name = [[coder decodeObjectOfClass:[NSString class] forKey:@"CIName"] copy];
    if ([coder containsValueForKey:@"CIEnabled"])
        _enabled = [coder decodeBoolForKey:@"CIEnabled"];
    for (NSString *key in self.inputKeys) {
        NSString *codingKey = [@"CI_" stringByAppendingString:key];
        if ([coder containsValueForKey:codingKey])
            [self setValue:[coder decodeObjectForKey:codingKey] forKey:key];
    }
    return self;
}

#pragma mark Making filters by name

+ (CIFilter *)filterWithName:(NSString *)name {
    if (name == nil)
        return nil;
    [registryLock() lock];
    _CIFilterEntry *entry = registry()[name];
    [registryLock() unlock];
    if (entry == nil)
        return nil;

    CIFilter *filter = entry.constructor ? [entry.constructor filterWithName:name] : [[entry.filterClass alloc] init];
    if (filter != nil && entry.constructor == nil && filter->_name == nil && ![NSStringFromClass([filter class]) isEqualToString:name])
        filter.name = name;
    [filter setDefaults];
    return filter;
}

+ (CIFilter *)filterWithName:(NSString *)name keysAndValues:(id)key0, ... {
    CIFilter *filter = [self filterWithName:name];
    if (filter == nil)
        return nil;
    va_list arguments;
    va_start(arguments, key0);
    for (id key = key0; key != nil; key = va_arg(arguments, id)) {
        id value = va_arg(arguments, id);
        [filter setValue:value forKey:key];
    }
    va_end(arguments);
    return filter;
}

+ (CIFilter *)filterWithName:(NSString *)name withInputParameters:(NSDictionary<NSString *, id> *)params {
    CIFilter *filter = [self filterWithName:name];
    [params enumerateKeysAndObjectsUsingBlock:^(NSString *key, id value, BOOL *stop) {
        [filter setValue:value forKey:key];
    }];
    return filter;
}

+ (void)registerFilterName:(NSString *)name constructor:(id<CIFilterConstructor>)anObject classAttributes:(NSDictionary<NSString *, id> *)attributes {
    if (name == nil || anObject == nil)
        return;
    _CIFilterEntry *entry = [_CIFilterEntry new];
    entry.constructor = anObject;
    entry.attributes = attributes ?: @{};
    [registryLock() lock];
    registry()[name] = entry;
    [registryLock() unlock];
}

#pragma mark The registry's names

/* The names of the filters in every category given (all of them with
   none), sorted. */
+ (NSArray<NSString *> *)filterNamesInCategories:(NSArray<NSString *> *)categories {
    NSMutableArray<NSString *> *names = [NSMutableArray array];
    [registryLock() lock];
    [registry() enumerateKeysAndObjectsUsingBlock:^(NSString *name, _CIFilterEntry *entry, BOOL *stop) {
        NSArray *filterCategories = entry.attributes[kCIAttributeFilterCategories];
        for (NSString *category in categories)
            if (![filterCategories containsObject:category])
                return;
        [names addObject:name];
    }];
    [registryLock() unlock];
    [names sortUsingSelector:@selector(compare:)];
    return names;
}

+ (NSArray<NSString *> *)filterNamesInCategory:(NSString *)category {
    return [self filterNamesInCategories:category ? @[ category ] : nil];
}

/* A built-in filter's name to show; nil for one registered with a
   constructor, as on the reference. */
+ (NSString *)localizedNameForFilterName:(NSString *)filterName {
    [registryLock() lock];
    _CIFilterEntry *entry = filterName ? registry()[filterName] : nil;
    [registryLock() unlock];
    if (entry == nil || entry.constructor != nil)
        return nil;
    return entry.attributes[kCIAttributeFilterDisplayName] ?: filterName;
}

+ (NSString *)localizedDescriptionForFilterName:(NSString *)filterName {
    [registryLock() lock];
    _CIFilterEntry *entry = filterName ? registry()[filterName] : nil;
    [registryLock() unlock];
    return entry.constructor == nil ? entry.attributes[kCIAttributeDescription] : nil;
}

/* No reference documentation is installed. */
+ (NSURL *)localizedReferenceDocumentationForFilterName:(NSString *)filterName {
    return nil;
}

/* A category's name to show; one not known is its own name. */
+ (NSString *)localizedNameForCategory:(NSString *)category {
    static NSDictionary<NSString *, NSString *> *names;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        names = @{
            kCICategoryDistortionEffect: @"Distortion Effect",
            kCICategoryGeometryAdjustment: @"Geometry Adjustment",
            kCICategoryCompositeOperation: @"Composite Operation",
            kCICategoryHalftoneEffect: @"Halftone Effect",
            kCICategoryColorAdjustment: @"Color Adjustment",
            kCICategoryColorEffect: @"Color Effect",
            kCICategoryTransition: @"Transition",
            kCICategoryTileEffect: @"Tile Effect",
            kCICategoryGenerator: @"Generator",
            kCICategoryReduction: @"Reduction",
            kCICategoryGradient: @"Gradient",
            kCICategoryStylize: @"Stylize",
            kCICategorySharpen: @"Sharpen",
            kCICategoryBlur: @"Blur",
            kCICategoryVideo: @"Video",
            kCICategoryStillImage: @"Still Image",
            kCICategoryInterlaced: @"Interlaced",
            kCICategoryNonSquarePixels: @"Non-Square Pixels",
            kCICategoryHighDynamicRange: @"High Dynamic Range",
            kCICategoryBuiltIn: @"Built-In",
            kCICategoryFilterGenerator: @"Filter Generator",
        };
    });
    return (category ? names[category] : nil) ?: category;
}

@end
