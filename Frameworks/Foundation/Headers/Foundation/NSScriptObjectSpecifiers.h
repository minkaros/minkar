// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSZone.h>

@class NSScriptClassDescription, NSScriptWhoseTest, NSString;

NS_ASSUME_NONNULL_BEGIN

/* Why a specifier could not be evaluated. */
enum {
    NSNoSpecifierError = 0,
    NSNoTopLevelContainersSpecifierError,
    NSContainerSpecifierError,
    NSUnknownKeySpecifierError,
    NSInvalidIndexSpecifierError,
    NSInternalSpecifierError,
    NSOperationNotSupportedForKeySpecifierError
};

typedef NS_ENUM(NSUInteger, NSInsertionPosition) {
    NSPositionAfter,
    NSPositionBefore,
    NSPositionBeginning,
    NSPositionEnd,
    NSPositionReplace
};

typedef NS_ENUM(NSUInteger, NSRelativePosition) {
    NSRelativeAfter = 0,
    NSRelativeBefore
};

typedef NS_ENUM(NSUInteger, NSWhoseSubelementIdentifier) {
    NSIndexSubelement = 0,
    NSEverySubelement = 1,
    NSMiddleSubelement = 2,
    NSRandomSubelement = 3,
    NSNoSubelement = 4
};

/* A reference to objects of a scriptable container by key ("window 1 of
   the application"): evaluated against its container's objects, the
   application at the top. */
@interface NSScriptObjectSpecifier : NSObject <NSCoding>

- (instancetype)initWithContainerSpecifier:(NSScriptObjectSpecifier *)container key:(NSString *)property;
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;

@property (nullable, assign) NSScriptObjectSpecifier *childSpecifier;
@property (nullable, retain) NSScriptObjectSpecifier *containerSpecifier;
@property BOOL containerIsObjectBeingTested;
@property BOOL containerIsRangeContainerObject;
@property (copy) NSString *key;
@property (nullable, retain) NSScriptClassDescription *containerClassDescription;
@property (nullable, readonly, retain) NSScriptClassDescription *keyClassDescription;
@property NSInteger evaluationErrorNumber;
@property (nullable, readonly, retain) NSScriptObjectSpecifier *evaluationErrorSpecifier;

- (nullable NSInteger *)indicesOfObjectsByEvaluatingWithContainer:(id)container count:(NSInteger *)numRefs NS_RETURNS_INNER_POINTER;
- (nullable id)objectsByEvaluatingWithContainers:(id)containers;
@property (nullable, readonly, retain) id objectsByEvaluatingSpecifier;

@end

@interface NSObject (NSScriptObjectSpecifiers)
@property (nullable, readonly, retain) NSScriptObjectSpecifier *objectSpecifier;
- (nullable NSArray<NSNumber *> *)indicesOfObjectsByEvaluatingObjectSpecifier:(NSScriptObjectSpecifier *)specifier;
@end

/* The object at an index (from 0; negative from the end). */
@interface NSIndexSpecifier : NSScriptObjectSpecifier
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property index:(NSInteger)index NS_DESIGNATED_INITIALIZER;
@property NSInteger index;
@end

/* The middle object. */
@interface NSMiddleSpecifier : NSScriptObjectSpecifier
@end

/* The first object with a name. */
@interface NSNameSpecifier : NSScriptObjectSpecifier
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property name:(NSString *)name NS_DESIGNATED_INITIALIZER;
@property (copy) NSString *name;
@end

/* A place to insert at, relative to an object. */
@interface NSPositionalSpecifier : NSObject
- (instancetype)initWithPosition:(NSInsertionPosition)position objectSpecifier:(NSScriptObjectSpecifier *)specifier NS_DESIGNATED_INITIALIZER;
@property (readonly) NSInsertionPosition position;
@property (readonly, retain) NSScriptObjectSpecifier *objectSpecifier;
- (void)setInsertionClassDescription:(NSScriptClassDescription *)classDescription;
- (void)evaluate;
@property (nullable, readonly, retain) id insertionContainer;
@property (nullable, readonly, copy) NSString *insertionKey;
@property (readonly) NSInteger insertionIndex;
@property (readonly) BOOL insertionReplaces;
@end

/* The value of a property. */
@interface NSPropertySpecifier : NSScriptObjectSpecifier
@end

/* A random object. */
@interface NSRandomSpecifier : NSScriptObjectSpecifier
@end

/* The objects from one to another. */
@interface NSRangeSpecifier : NSScriptObjectSpecifier
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property startSpecifier:(nullable NSScriptObjectSpecifier *)startSpec endSpecifier:(nullable NSScriptObjectSpecifier *)endSpec NS_DESIGNATED_INITIALIZER;
@property (nullable, retain) NSScriptObjectSpecifier *startSpecifier;
@property (nullable, retain) NSScriptObjectSpecifier *endSpecifier;
@end

/* The object before or after another. */
@interface NSRelativeSpecifier : NSScriptObjectSpecifier
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property relativePosition:(NSRelativePosition)relPos baseSpecifier:(nullable NSScriptObjectSpecifier *)baseSpecifier NS_DESIGNATED_INITIALIZER;
@property NSRelativePosition relativePosition;
@property (nullable, retain) NSScriptObjectSpecifier *baseSpecifier;
@end

/* The object with a unique ID. */
@interface NSUniqueIDSpecifier : NSScriptObjectSpecifier
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property uniqueID:(id)uniqueID NS_DESIGNATED_INITIALIZER;
@property (copy) id uniqueID;
@end

/* The objects that pass a test ("windows whose name is ..."). */
@interface NSWhoseSpecifier : NSScriptObjectSpecifier
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithContainerClassDescription:(NSScriptClassDescription *)classDesc containerSpecifier:(nullable NSScriptObjectSpecifier *)container key:(NSString *)property test:(NSScriptWhoseTest *)test NS_DESIGNATED_INITIALIZER;
@property (retain) NSScriptWhoseTest *test;
@property NSWhoseSubelementIdentifier startSubelementIdentifier;
@property NSInteger startSubelementIndex;
@property NSWhoseSubelementIdentifier endSubelementIdentifier;
@property NSInteger endSubelementIndex;
@end

NS_ASSUME_NONNULL_END
