// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSScriptObjectSpecifiers.h>
#import <Foundation/NSScriptClassDescription.h>
#import <Foundation/NSScriptWhoseTests.h>
#import <Foundation/Foundation.h>

/* The object a "whose" test is evaluated for, while it is. */
static NSString *const NSScriptObjectBeingTestedKey = @"NSScriptObjectBeingTested";

static id objectBeingTested(void) {
    return [NSThread currentThread].threadDictionary[NSScriptObjectBeingTestedKey];
}

/* The top-level container: the application. */
static id topLevelContainer(void) {
    Class application = NSClassFromString(@"NSApplication");
    return [application respondsToSelector:@selector(sharedApplication)] ? [application performSelector:@selector(sharedApplication)] : nil;
}

/* A container's value for a key, nil when it has none. */
static id valueForKeySafely(id container, NSString *key) {
    if (container == nil || key == nil)
        return nil;
    @try {
        return [container valueForKey:key];
    } @catch (NSException *exception) {
        return nil;
    }
}

@implementation NSScriptObjectSpecifier {
    NSScriptClassDescription *_containerClassDescription;
    NSScriptObjectSpecifier *_containerSpecifier;
    __weak NSScriptObjectSpecifier *_childSpecifier;
    NSString *_key;
    NSScriptObjectSpecifier *_evaluationErrorSpecifier;
}

@synthesize containerIsObjectBeingTested = _containerIsObjectBeingTested;
@synthesize containerIsRangeContainerObject = _containerIsRangeContainerObject;
@synthesize evaluationErrorNumber = _evaluationErrorNumber;

- (instancetype)init {
    return [self initWithContainerClassDescription:nil containerSpecifier:nil key:@""];
}

- (instancetype)initWithContainerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:container.keyClassDescription containerSpecifier:container key:property];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    if ((self = [super init]) != nil) {
        _containerClassDescription = classDesc;
        _key = [property copy] ?: @"";
        self.containerSpecifier = container;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) != nil) {
        _key = [[coder decodeObjectForKey:@"NSKey"] copy] ?: @"";
        self.containerSpecifier = [coder decodeObjectForKey:@"NSContainerSpecifier"];
        _containerIsObjectBeingTested = [coder decodeBoolForKey:@"NSContainerIsObjectBeingTested"];
        _containerIsRangeContainerObject = [coder decodeBoolForKey:@"NSContainerIsRangeContainerObject"];
        NSString *className = [coder decodeObjectForKey:@"NSContainerClassName"];
        if (className != nil && NSClassFromString(className) != Nil)
            _containerClassDescription = [NSScriptClassDescription classDescriptionForClass:NSClassFromString(className)];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_key forKey:@"NSKey"];
    [coder encodeObject:_containerSpecifier forKey:@"NSContainerSpecifier"];
    [coder encodeBool:_containerIsObjectBeingTested forKey:@"NSContainerIsObjectBeingTested"];
    [coder encodeBool:_containerIsRangeContainerObject forKey:@"NSContainerIsRangeContainerObject"];
    [coder encodeObject:_containerClassDescription.implementationClassName forKey:@"NSContainerClassName"];
}

#pragma mark Properties

- (NSString *)key { return _key; }
- (void)setKey:(NSString *)key { _key = [key copy] ?: @""; }
- (NSScriptClassDescription *)containerClassDescription { return _containerClassDescription; }
- (void)setContainerClassDescription:(NSScriptClassDescription *)description { _containerClassDescription = description; }
- (NSScriptObjectSpecifier *)childSpecifier { return _childSpecifier; }
- (void)setChildSpecifier:(NSScriptObjectSpecifier *)child { _childSpecifier = child; }
- (NSScriptObjectSpecifier *)containerSpecifier { return _containerSpecifier; }
- (NSScriptObjectSpecifier *)evaluationErrorSpecifier { return _evaluationErrorSpecifier; }

/* A container's child is the specifier it contains. */
- (void)setContainerSpecifier:(NSScriptObjectSpecifier *)container {
    _containerSpecifier = container;
    container.childSpecifier = self;
}

- (NSScriptClassDescription *)keyClassDescription {
    return [_containerClassDescription classDescriptionForKey:_key];
}

#pragma mark Description

/* "orderedWindows 1": the key and what picks the object (indexes from 1),
   then " of" its container's. */
- (NSString *)_selectionDescription {
    return _key;
}

- (NSString *)_chainDescription {
    NSString *own = [self _selectionDescription];
    if (_containerSpecifier != nil)
        return [NSString stringWithFormat:@"%@ of %@", own, [_containerSpecifier _chainDescription]];
    return own;
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@: %@>", NSStringFromClass([self class]), [self _chainDescription]];
}

#pragma mark Evaluation

/* All the objects: no indices (a count of -1). */
- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    if (numRefs)
        *numRefs = -1;
    return NULL;
}

/* The key's value of each container, narrowed to the indices the
   specifier picks: one object, several, or nil (with an error number). */
- (id)objectsByEvaluatingWithContainers:(id)containers {
    _evaluationErrorNumber = NSNoSpecifierError;
    _evaluationErrorSpecifier = nil;
    if ([containers isKindOfClass:[NSArray class]]) {
        NSMutableArray *results = [NSMutableArray array];
        for (id container in containers) {
            id result = [self objectsByEvaluatingWithContainers:container];
            if ([result isKindOfClass:[NSArray class]])
                [results addObjectsFromArray:result];
            else if (result != nil)
                [results addObject:result];
        }
        return results;
    }

    id value = valueForKeySafely(containers, _key);
    if (value == nil) {
        _evaluationErrorNumber = NSUnknownKeySpecifierError;
        _evaluationErrorSpecifier = self;
        return nil;
    }
    if (![value isKindOfClass:[NSArray class]])
        return value;

    NSArray *objects = value;
    NSInteger count = 0;
    NSInteger *indices = [self indicesOfObjectsByEvaluatingWithContainer:containers count:&count];
    if (count == -1)
        return objects;
    if (indices == NULL || count == 0) {
        if (_evaluationErrorNumber == NSNoSpecifierError)
            _evaluationErrorNumber = NSInternalSpecifierError;
        _evaluationErrorSpecifier = self;
        return nil;
    }
    if (count == 1)
        return indices[0] >= 0 && (NSUInteger)indices[0] < objects.count ? objects[(NSUInteger)indices[0]] : nil;
    NSMutableArray *picked = [NSMutableArray array];
    for (NSInteger i = 0; i < count; i++)
        if (indices[i] >= 0 && (NSUInteger)indices[i] < objects.count)
            [picked addObject:objects[(NSUInteger)indices[i]]];
    return picked;
}

/* From the top: the container's objects (the application's, the object
   being tested's), then this specifier's of them. */
- (id)objectsByEvaluatingSpecifier {
    id containers;
    if (_containerIsObjectBeingTested)
        containers = objectBeingTested();
    else if (_containerSpecifier != nil) {
        containers = _containerSpecifier.objectsByEvaluatingSpecifier;
        if (containers == nil) {
            _evaluationErrorNumber = _containerSpecifier.evaluationErrorNumber ?: NSContainerSpecifierError;
            _evaluationErrorSpecifier = _containerSpecifier.evaluationErrorSpecifier ?: _containerSpecifier;
            return nil;
        }
    } else
        containers = topLevelContainer();
    if (containers == nil) {
        _evaluationErrorNumber = NSNoTopLevelContainersSpecifierError;
        _evaluationErrorSpecifier = self;
        return nil;
    }
    return [self objectsByEvaluatingWithContainers:containers];
}

/* The objects of the key in a container, for subclasses. */
- (NSArray *)_objectsInContainer:(id)container {
    id value = valueForKeySafely(container, _key);
    return [value isKindOfClass:[NSArray class]] ? value : (value ? @[ value ] : nil);
}

/* One index, kept for the caller (indices of one). */
- (NSInteger *)_singleIndex:(NSInteger)index count:(NSInteger *)numRefs {
    static __thread NSInteger storage;
    storage = index;
    if (numRefs)
        *numRefs = 1;
    return &storage;
}

@end

@implementation NSObject (NSScriptObjectSpecifiers)

- (NSScriptObjectSpecifier *)objectSpecifier {
    return nil;
}

- (NSArray<NSNumber *> *)indicesOfObjectsByEvaluatingObjectSpecifier:(NSScriptObjectSpecifier *)specifier {
    return nil;
}

@end

#pragma mark - Index

@implementation NSIndexSpecifier {
    NSInteger _index;
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:classDesc containerSpecifier:container key:property index:0];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property index:(NSInteger)index {
    if ((self = [super initWithContainerClassDescription:classDesc containerSpecifier:container key:property]) != nil)
        _index = index;
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil)
        _index = [coder decodeIntegerForKey:@"NSIndex"];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeInteger:_index forKey:@"NSIndex"];
}

- (NSInteger)index { return _index; }
- (void)setIndex:(NSInteger)index { _index = index; }

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"%@ %ld", self.key, (long)(_index >= 0 ? _index + 1 : _index)];
}

- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSArray *objects = [self _objectsInContainer:container];
    NSInteger index = _index < 0 ? (NSInteger)objects.count + _index : _index;
    if (index < 0 || index >= (NSInteger)objects.count) {
        self.evaluationErrorNumber = NSInvalidIndexSpecifierError;
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    return [self _singleIndex:index count:numRefs];
}

@end

#pragma mark - Middle, random

@implementation NSMiddleSpecifier

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"middle %@", self.key];
}

- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSArray *objects = [self _objectsInContainer:container];
    if (objects.count == 0) {
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    return [self _singleIndex:(NSInteger)(objects.count - 1) / 2 count:numRefs];
}

@end

@implementation NSRandomSpecifier

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"any %@", self.key];
}

- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSArray *objects = [self _objectsInContainer:container];
    if (objects.count == 0) {
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    return [self _singleIndex:(NSInteger)arc4random_uniform((uint32_t)objects.count) count:numRefs];
}

@end

#pragma mark - Name, unique ID

/* The first object whose value for a key equals a value. */
static NSInteger indexOfObjectWithValue(NSArray *objects, NSString *key, id value) {
    for (NSUInteger i = 0; i < objects.count; i++)
        if ([valueForKeySafely(objects[i], key) isEqual:value])
            return (NSInteger)i;
    return NSNotFound;
}

@implementation NSNameSpecifier {
    NSString *_name;
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:classDesc containerSpecifier:container key:property name:@""];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property name:(NSString *)name {
    if ((self = [super initWithContainerClassDescription:classDesc containerSpecifier:container key:property]) != nil)
        _name = [name copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil)
        _name = [[coder decodeObjectForKey:@"NSName"] copy];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeObject:_name forKey:@"NSName"];
}

- (NSString *)name { return _name; }
- (void)setName:(NSString *)name { _name = [name copy]; }

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"%@ named \"%@\"", self.key, _name];
}

/* By the name the objects' class gives scripting ("pnam"), or "name". */
- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSString *nameKey = [self.keyClassDescription keyWithAppleEventCode:'pnam'] ?: @"name";
    NSInteger index = indexOfObjectWithValue([self _objectsInContainer:container], nameKey, _name);
    if (index == NSNotFound) {
        self.evaluationErrorNumber = NSInternalSpecifierError;
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    return [self _singleIndex:index count:numRefs];
}

@end

@implementation NSUniqueIDSpecifier {
    id _uniqueID;
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:classDesc containerSpecifier:container key:property uniqueID:@0];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property uniqueID:(id)uniqueID {
    if ((self = [super initWithContainerClassDescription:classDesc containerSpecifier:container key:property]) != nil)
        _uniqueID = [uniqueID copy];
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil)
        _uniqueID = [[coder decodeObjectForKey:@"NSUniqueID"] copy];
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeObject:_uniqueID forKey:@"NSUniqueID"];
}

- (id)uniqueID { return _uniqueID; }
- (void)setUniqueID:(id)uniqueID { _uniqueID = [uniqueID copy]; }

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"%@ with an ID of \"%@\"", self.key, _uniqueID];
}

/* By the ID the objects' class gives scripting ("ID  "), or "uniqueID". */
- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSString *idKey = [self.keyClassDescription keyWithAppleEventCode:'ID  '] ?: @"uniqueID";
    NSInteger index = indexOfObjectWithValue([self _objectsInContainer:container], idKey, _uniqueID);
    if (index == NSNotFound) {
        self.evaluationErrorNumber = NSInternalSpecifierError;
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    return [self _singleIndex:index count:numRefs];
}

@end

#pragma mark - Property

@implementation NSPropertySpecifier

/* A property's value as it is (one object, a list for a to-many key). */
- (id)objectsByEvaluatingWithContainers:(id)containers {
    if ([containers isKindOfClass:[NSArray class]])
        return [super objectsByEvaluatingWithContainers:containers];
    self.evaluationErrorNumber = NSNoSpecifierError;
    id value = valueForKeySafely(containers, self.key);
    if (value == nil)
        self.evaluationErrorNumber = NSUnknownKeySpecifierError;
    return value;
}

@end

#pragma mark - Range

@implementation NSRangeSpecifier {
    NSScriptObjectSpecifier *_startSpecifier;
    NSScriptObjectSpecifier *_endSpecifier;
    NSInteger _range[2];
    NSInteger *_indices;
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:classDesc containerSpecifier:container key:property startSpecifier:nil endSpecifier:nil];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property startSpecifier:(NSScriptObjectSpecifier *)startSpec endSpecifier:(NSScriptObjectSpecifier *)endSpec {
    if ((self = [super initWithContainerClassDescription:classDesc containerSpecifier:container key:property]) != nil) {
        self.startSpecifier = startSpec;
        self.endSpecifier = endSpec;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil) {
        self.startSpecifier = [coder decodeObjectForKey:@"NSStartSpecifier"];
        self.endSpecifier = [coder decodeObjectForKey:@"NSEndSpecifier"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeObject:_startSpecifier forKey:@"NSStartSpecifier"];
    [coder encodeObject:_endSpecifier forKey:@"NSEndSpecifier"];
}

- (void)dealloc {
    free(_indices);
}

- (NSScriptObjectSpecifier *)startSpecifier { return _startSpecifier; }
- (NSScriptObjectSpecifier *)endSpecifier { return _endSpecifier; }

/* The ends are evaluated in the range's container. */
- (void)setStartSpecifier:(NSScriptObjectSpecifier *)specifier {
    _startSpecifier = specifier;
    specifier.containerIsRangeContainerObject = YES;
}

- (void)setEndSpecifier:(NSScriptObjectSpecifier *)specifier {
    _endSpecifier = specifier;
    specifier.containerIsRangeContainerObject = YES;
}

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"%@ from %@ to %@", self.key, [_startSpecifier _selectionDescription] ?: @"beginning",
            [_endSpecifier _selectionDescription] ?: @"end"];
}

/* An end's index in the container's objects: its own (one), or the first
   or last when there is none. */
- (NSInteger)_indexOf:(NSScriptObjectSpecifier *)end container:(id)container objects:(NSArray *)objects last:(BOOL)last {
    if (end == nil)
        return last ? (NSInteger)objects.count - 1 : 0;
    NSInteger count = 0;
    NSInteger *index = [end indicesOfObjectsByEvaluatingWithContainer:container count:&count];
    if (count == 1 && index != NULL)
        return index[0];
    id object = [end objectsByEvaluatingWithContainers:container];
    NSUInteger found = object ? [objects indexOfObjectIdenticalTo:object] : NSNotFound;
    return found == NSNotFound ? NSNotFound : (NSInteger)found;
}

- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSArray *objects = [self _objectsInContainer:container];
    NSInteger start = [self _indexOf:_startSpecifier container:container objects:objects last:NO];
    NSInteger end = [self _indexOf:_endSpecifier container:container objects:objects last:YES];
    if (start == NSNotFound || end == NSNotFound || objects.count == 0) {
        self.evaluationErrorNumber = NSInvalidIndexSpecifierError;
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    if (start > end) {
        NSInteger swap = start;
        start = end;
        end = swap;
    }
    free(_indices);
    _indices = malloc(sizeof(NSInteger) * (size_t)(end - start + 1));
    for (NSInteger i = start; i <= end; i++)
        _indices[i - start] = i;
    if (numRefs)
        *numRefs = end - start + 1;
    return _indices;
}

@end

#pragma mark - Relative

@implementation NSRelativeSpecifier {
    NSRelativePosition _relativePosition;
    NSScriptObjectSpecifier *_baseSpecifier;
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:classDesc containerSpecifier:container key:property relativePosition:NSRelativeAfter baseSpecifier:nil];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property relativePosition:(NSRelativePosition)relPos baseSpecifier:(NSScriptObjectSpecifier *)baseSpecifier {
    if ((self = [super initWithContainerClassDescription:classDesc containerSpecifier:container key:property]) != nil) {
        _relativePosition = relPos;
        _baseSpecifier = baseSpecifier;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil) {
        _relativePosition = (NSRelativePosition)[coder decodeIntegerForKey:@"NSRelativePosition"];
        _baseSpecifier = [coder decodeObjectForKey:@"NSBaseSpecifier"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeInteger:(NSInteger)_relativePosition forKey:@"NSRelativePosition"];
    [coder encodeObject:_baseSpecifier forKey:@"NSBaseSpecifier"];
}

- (NSRelativePosition)relativePosition { return _relativePosition; }
- (void)setRelativePosition:(NSRelativePosition)position { _relativePosition = position; }
- (NSScriptObjectSpecifier *)baseSpecifier { return _baseSpecifier; }
- (void)setBaseSpecifier:(NSScriptObjectSpecifier *)specifier { _baseSpecifier = specifier; }

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"%@ %@ %@", self.key, _relativePosition == NSRelativeBefore ? @"before" : @"after", [_baseSpecifier _selectionDescription]];
}

/* The object just before or after the base one. */
- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSArray *objects = [self _objectsInContainer:container];
    id base = _baseSpecifier.objectsByEvaluatingSpecifier;
    NSUInteger at = base ? [objects indexOfObjectIdenticalTo:base] : NSNotFound;
    NSInteger index = at == NSNotFound ? NSNotFound : (NSInteger)at + (_relativePosition == NSRelativeBefore ? -1 : 1);
    if (index == NSNotFound || index < 0 || index >= (NSInteger)objects.count) {
        self.evaluationErrorNumber = NSInvalidIndexSpecifierError;
        if (numRefs)
            *numRefs = 0;
        return NULL;
    }
    return [self _singleIndex:index count:numRefs];
}

@end

#pragma mark - Whose

@implementation NSWhoseSpecifier {
    NSScriptWhoseTest *_test;
    NSInteger *_indices;
}

@synthesize startSubelementIdentifier = _startSubelementIdentifier;
@synthesize startSubelementIndex = _startSubelementIndex;
@synthesize endSubelementIdentifier = _endSubelementIdentifier;
@synthesize endSubelementIndex = _endSubelementIndex;

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property {
    return [self initWithContainerClassDescription:classDesc containerSpecifier:container key:property test:[NSScriptWhoseTest new]];
}

- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property test:(NSScriptWhoseTest *)test {
    if ((self = [super initWithContainerClassDescription:classDesc containerSpecifier:container key:property]) != nil) {
        _test = test;
        _startSubelementIdentifier = NSEverySubelement;
        _endSubelementIdentifier = NSNoSubelement;
    }
    return self;
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil) {
        _test = [coder decodeObjectForKey:@"NSTest"];
        _startSubelementIdentifier = (NSWhoseSubelementIdentifier)[coder decodeIntegerForKey:@"NSStartSubelementIdentifier"];
        _startSubelementIndex = [coder decodeIntegerForKey:@"NSStartSubelementIndex"];
        _endSubelementIdentifier = (NSWhoseSubelementIdentifier)[coder decodeIntegerForKey:@"NSEndSubelementIdentifier"];
        _endSubelementIndex = [coder decodeIntegerForKey:@"NSEndSubelementIndex"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [super encodeWithCoder:coder];
    [coder encodeObject:_test forKey:@"NSTest"];
    [coder encodeInteger:(NSInteger)_startSubelementIdentifier forKey:@"NSStartSubelementIdentifier"];
    [coder encodeInteger:_startSubelementIndex forKey:@"NSStartSubelementIndex"];
    [coder encodeInteger:(NSInteger)_endSubelementIdentifier forKey:@"NSEndSubelementIdentifier"];
    [coder encodeInteger:_endSubelementIndex forKey:@"NSEndSubelementIndex"];
}

- (void)dealloc {
    free(_indices);
}

- (NSScriptWhoseTest *)test { return _test; }
- (void)setTest:(NSScriptWhoseTest *)test { _test = test; }

- (NSString *)_selectionDescription {
    return [NSString stringWithFormat:@"%@ whose %@", self.key, _test];
}

/* The objects passing the test, then the subelement asked for of them
   (every one, an index, the middle, any). */
- (NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs {
    NSArray *objects = [self _objectsInContainer:container];
    NSMutableArray<NSNumber *> *passing = [NSMutableArray array];
    NSMutableDictionary *thread = [NSThread currentThread].threadDictionary;
    id previous = thread[NSScriptObjectBeingTestedKey];
    for (NSUInteger i = 0; i < objects.count; i++) {
        thread[NSScriptObjectBeingTestedKey] = objects[i];
        if ([_test isTrue])
            [passing addObject:@(i)];
    }
    if (previous)
        thread[NSScriptObjectBeingTestedKey] = previous;
    else
        [thread removeObjectForKey:NSScriptObjectBeingTestedKey];

    NSArray<NSNumber *> *picked = passing;
    NSInteger count = (NSInteger)passing.count;
    switch (_startSubelementIdentifier) {
    case NSIndexSubelement: {
        NSInteger index = _startSubelementIndex < 0 ? count + _startSubelementIndex : _startSubelementIndex;
        picked = index >= 0 && index < count ? @[ passing[(NSUInteger)index] ] : @[];
        break;
    }
    case NSMiddleSubelement:
        picked = count ? @[ passing[(NSUInteger)(count - 1) / 2] ] : @[];
        break;
    case NSRandomSubelement:
        picked = count ? @[ passing[arc4random_uniform((uint32_t)count)] ] : @[];
        break;
    default:
        break;
    }
    free(_indices);
    _indices = malloc(sizeof(NSInteger) * MAX(picked.count, (NSUInteger)1));
    for (NSUInteger i = 0; i < picked.count; i++)
        _indices[i] = picked[i].integerValue;
    if (numRefs)
        *numRefs = (NSInteger)picked.count;
    if (picked.count == 0)
        self.evaluationErrorNumber = NSInvalidIndexSpecifierError;
    return picked.count ? _indices : NULL;
}

@end

#pragma mark - Positional

@implementation NSPositionalSpecifier {
    NSInsertionPosition _position;
    NSScriptObjectSpecifier *_objectSpecifier;
    NSScriptClassDescription *_insertionClassDescription;
    id _insertionContainer;
    NSString *_insertionKey;
    NSInteger _insertionIndex;
    BOOL _insertionReplaces;
    BOOL _evaluated;
}

- (instancetype)init {
    return [self initWithPosition:NSPositionEnd objectSpecifier:[NSScriptObjectSpecifier new]];
}

- (instancetype)initWithPosition:(NSInsertionPosition)position objectSpecifier:(NSScriptObjectSpecifier *)specifier {
    if ((self = [super init]) != nil) {
        _position = position;
        _objectSpecifier = specifier;
        _insertionIndex = -1;
    }
    return self;
}

- (NSInsertionPosition)position { return _position; }
- (NSScriptObjectSpecifier *)objectSpecifier { return _objectSpecifier; }

- (void)setInsertionClassDescription:(NSScriptClassDescription *)classDescription {
    _insertionClassDescription = classDescription;
    _evaluated = NO;
}

/* Where to insert: before or after (or replacing) the object specified,
   in its container's key; or at the beginning or end of the to-many key
   of the object specified that holds the class to insert. */
- (void)evaluate {
    _evaluated = YES;
    _insertionContainer = nil;
    _insertionKey = nil;
    _insertionIndex = -1;
    _insertionReplaces = (_position == NSPositionReplace);

    if (_position == NSPositionBeginning || _position == NSPositionEnd) {
        id container = _objectSpecifier.objectsByEvaluatingSpecifier;
        NSScriptClassDescription *description = [NSScriptClassDescription classDescriptionForClass:[container class]];
        NSString *key = nil;
        for (NSString *candidate in description.toManyRelationshipKeys)
            if (_insertionClassDescription == nil || [[description classDescriptionForKey:candidate] isEqual:_insertionClassDescription])
                { key = candidate; break; }
        if (container == nil || key == nil)
            return;
        _insertionContainer = container;
        _insertionKey = key;
        _insertionIndex = (_position == NSPositionBeginning) ? 0 : (NSInteger)[valueForKeySafely(container, key) count];
        return;
    }

    NSScriptObjectSpecifier *containerSpecifier = _objectSpecifier.containerSpecifier;
    id container = containerSpecifier ? containerSpecifier.objectsByEvaluatingSpecifier : topLevelContainer();
    id object = _objectSpecifier.objectsByEvaluatingSpecifier;
    NSArray *objects = valueForKeySafely(container, _objectSpecifier.key);
    NSUInteger index = ([objects isKindOfClass:[NSArray class]] && object) ? [objects indexOfObjectIdenticalTo:object] : NSNotFound;
    if (index == NSNotFound)
        return;
    _insertionContainer = container;
    _insertionKey = _objectSpecifier.key;
    _insertionIndex = (NSInteger)index + (_position == NSPositionAfter ? 1 : 0);
}

- (id)insertionContainer {
    if (!_evaluated)
        [self evaluate];
    return _insertionContainer;
}

- (NSString *)insertionKey {
    if (!_evaluated)
        [self evaluate];
    return _insertionKey;
}

- (NSInteger)insertionIndex {
    if (!_evaluated)
        [self evaluate];
    return _insertionIndex;
}

- (BOOL)insertionReplaces {
    if (!_evaluated)
        [self evaluate];
    return _insertionReplaces;
}

@end

#pragma mark - Whose tests

@implementation NSScriptWhoseTest

- (instancetype)init {
    return [super init];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    return [super init];
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

- (BOOL)isTrue {
    return NO;
}

@end

typedef NS_ENUM(NSInteger, _NSLogicalOperator) { _NSLogicalAnd, _NSLogicalOr, _NSLogicalNot };

@implementation NSLogicalTest {
    _NSLogicalOperator _operator;
    NSArray<NSScriptWhoseTest *> *_tests;
}

- (instancetype)init {
    return [self initAndTestWithTests:@[]];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil) {
        _operator = (_NSLogicalOperator)[coder decodeIntegerForKey:@"NSOperator"];
        _tests = [coder decodeObjectForKey:@"NSTests"] ?: @[];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeInteger:_operator forKey:@"NSOperator"];
    [coder encodeObject:_tests forKey:@"NSTests"];
}

- (instancetype)initAndTestWithTests:(NSArray<NSSpecifierTest *> *)subTests {
    if ((self = [super init]) != nil) {
        _operator = _NSLogicalAnd;
        _tests = [subTests copy];
    }
    return self;
}

- (instancetype)initOrTestWithTests:(NSArray<NSSpecifierTest *> *)subTests {
    if ((self = [super init]) != nil) {
        _operator = _NSLogicalOr;
        _tests = [subTests copy];
    }
    return self;
}

- (instancetype)initNotTestWithTest:(NSScriptWhoseTest *)subTest {
    if ((self = [super init]) != nil) {
        _operator = _NSLogicalNot;
        _tests = subTest ? @[ subTest ] : @[];
    }
    return self;
}

- (BOOL)isTrue {
    switch (_operator) {
    case _NSLogicalAnd:
        for (NSScriptWhoseTest *test in _tests)
            if (![test isTrue])
                return NO;
        return YES;
    case _NSLogicalOr:
        for (NSScriptWhoseTest *test in _tests)
            if ([test isTrue])
                return YES;
        return NO;
    case _NSLogicalNot:
        return ![_tests.firstObject isTrue];
    }
    return NO;
}

- (NSString *)description {
    NSString *joiner = _operator == _NSLogicalOr ? @" or " : @" and ";
    if (_operator == _NSLogicalNot)
        return [NSString stringWithFormat:@"not (%@)", _tests.firstObject];
    return [[_tests valueForKey:@"description"] componentsJoinedByString:joiner];
}

@end

@implementation NSSpecifierTest {
    NSScriptObjectSpecifier *_specifier;
    NSTestComparisonOperation _operator;
    id _object;
}

- (instancetype)init {
    return [self initWithObjectSpecifier:nil comparisonOperator:NSEqualToComparison testObject:nil];
}

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super initWithCoder:coder]) != nil) {
        _specifier = [coder decodeObjectForKey:@"NSObject1"];
        _operator = (NSTestComparisonOperation)[coder decodeIntegerForKey:@"NSComparisonOperator"];
        _object = [coder decodeObjectForKey:@"NSObject2"];
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
    [coder encodeObject:_specifier forKey:@"NSObject1"];
    [coder encodeInteger:(NSInteger)_operator forKey:@"NSComparisonOperator"];
    [coder encodeObject:_object forKey:@"NSObject2"];
}

/* The specifier is relative to the object being tested. */
- (instancetype)initWithObjectSpecifier:(NSScriptObjectSpecifier *)obj1 comparisonOperator:(NSTestComparisonOperation)compOp testObject:(id)obj2 {
    if ((self = [super init]) != nil) {
        _specifier = obj1;
        _operator = compOp;
        _object = obj2;
        if (obj1.containerSpecifier == nil)
            obj1.containerIsObjectBeingTested = YES;
    }
    return self;
}

- (BOOL)isTrue {
    id value = _specifier ? _specifier.objectsByEvaluatingSpecifier : objectBeingTested();
    id other = [_object isKindOfClass:[NSScriptObjectSpecifier class]] ? [(NSScriptObjectSpecifier *)_object objectsByEvaluatingSpecifier] : _object;
    switch (_operator) {
    case NSEqualToComparison: return [value isEqualTo:other];
    case NSLessThanOrEqualToComparison: return [value isLessThanOrEqualTo:other];
    case NSLessThanComparison: return [value isLessThan:other];
    case NSGreaterThanOrEqualToComparison: return [value isGreaterThanOrEqualTo:other];
    case NSGreaterThanComparison: return [value isGreaterThan:other];
    case NSBeginsWithComparison:
        return [value isKindOfClass:[NSString class]] && [other isKindOfClass:[NSString class]] && [value hasPrefix:other];
    case NSEndsWithComparison:
        return [value isKindOfClass:[NSString class]] && [other isKindOfClass:[NSString class]] && [value hasSuffix:other];
    case NSContainsComparison:
        return [value doesContain:other];
    }
    return NO;
}

- (NSString *)description {
    static NSArray *names;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ names = @[ @"==", @"<=", @"<", @">=", @">", @"begins with", @"ends with", @"contains" ]; });
    return [NSString stringWithFormat:@"%@ %@ %@", _specifier.key ?: @"it", (NSUInteger)_operator < names.count ? names[_operator] : @"?", _object];
}

@end
