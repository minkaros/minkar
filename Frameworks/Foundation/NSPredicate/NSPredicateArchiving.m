// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// Predicates and expressions as the reference archives them (in nibs and
// documents): each expression an object of a class of its kind with its
// NSExpressionType and parts, each comparison an operator object. Read
// into the public classes.

#import <Foundation/Foundation.h>
#import "NSPredicateArchiving.h"

typedef NS_ENUM(NSInteger, _NSArchivedExpressionType) {
    _NSArchivedConstantValue = 0,
    _NSArchivedEvaluatedObject = 1,
    _NSArchivedVariable = 2,
    _NSArchivedKeyPath = 3,
    _NSArchivedFunction = 4,
    _NSArchivedKeyPathSpecifier = 10,
    _NSArchivedAggregate = 14,
};

/* A function expression's key path: the specifier's ("parent.type"), or
   a key path expression's own. */
static NSString *keyPathOf(id argument) {
    if ([argument isKindOfClass:[NSString class]])
        return argument;
    if ([argument isKindOfClass:[NSExpression class]] && [(NSExpression *)argument expressionType] == NSKeyPathExpressionType)
        return [(NSExpression *)argument keyPath];
    return nil;
}

/* Stands for the reference's expression classes while unarchiving: what
   it decodes is the public expression. */
@interface _NSArchivedExpression : NSObject <NSCoding>
@end

@implementation _NSArchivedExpression

- (instancetype)initWithCoder:(NSCoder *)coder {
    NSInteger type = [coder decodeIntegerForKey:@"NSExpressionType"];
    NSExpression *result = nil;

    switch (type) {
    case _NSArchivedConstantValue:
        result = [NSExpression expressionForConstantValue:[coder decodeObjectForKey:@"NSConstantValue"]];
        break;
    case _NSArchivedEvaluatedObject:
        result = [NSExpression expressionForEvaluatedObject];
        break;
    case _NSArchivedVariable:
        result = [NSExpression expressionForVariable:[coder decodeObjectForKey:@"NSVariable"] ?: @""];
        break;
    case _NSArchivedKeyPathSpecifier: {
        /* The key path itself, as a key path expression. */
        NSString *keyPath = [coder decodeObjectForKey:@"NSKeyPath"];
        result = keyPath ? [NSExpression expressionForKeyPath:keyPath] : nil;
        break;
    }
    case _NSArchivedKeyPath:
    case _NSArchivedFunction: {
        NSExpression *operand = [coder decodeObjectForKey:@"NSOperand"];
        NSString *selector = [coder decodeObjectForKey:@"NSSelectorName"];
        NSArray *arguments = [coder decodeObjectForKey:@"NSArguments"];
        if ([selector isEqualToString:@"valueForKeyPath:"] && arguments.count == 1) {
            NSString *keyPath = keyPathOf(arguments[0]);
            if (keyPath == nil)
                break;
            if (operand == nil || operand.expressionType == NSEvaluatedObjectExpressionType)
                result = [NSExpression expressionForKeyPath:keyPath];
            else if (operand.expressionType == NSKeyPathExpressionType)
                result = [NSExpression expressionForKeyPath:[NSString stringWithFormat:@"%@.%@", operand.keyPath, keyPath]];
            else
                result = [NSExpression expressionWithFormat:[NSString stringWithFormat:@"%@.%@", operand.description, keyPath]];
        } else if (selector != nil) {
            NSString *name = [selector hasSuffix:@":"] ? [selector substringToIndex:selector.length - 1] : selector;
            result = [NSExpression expressionForFunction:[name stringByAppendingString:@":"] arguments:arguments ?: @[]];
        }
        break;
    }
    case _NSArchivedAggregate: {
        NSArray *collection = [coder decodeObjectForKey:@"NSCollection"];
        result = collection ? [NSExpression expressionForAggregate:collection] : nil;
        break;
    }
    default:
        break;
    }
    return (id)result;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

@end

@implementation _NSArchivedPredicateOperator

@synthesize operatorType = _operatorType, modifier = _modifier, options = _options, negate = _negate, customSelector = _customSelector;

- (instancetype)initWithCoder:(NSCoder *)coder {
    if ((self = [super init]) != nil) {
        _operatorType = [coder decodeIntegerForKey:@"NSOperatorType"];
        _modifier = [coder decodeIntegerForKey:@"NSModifier"];
        _options = (NSUInteger)[coder decodeIntegerForKey:@"NSOptions"];
        _negate = [coder decodeBoolForKey:@"NSNegate"];
        NSString *selector = [coder decodeObjectForKey:@"NSSelectorName"];
        _customSelector = selector ? NSSelectorFromString(selector) : NULL;
    }
    return self;
}

- (void)encodeWithCoder:(NSCoder *)coder {
}

/* The public operator type: an equality negated is "not equal". */
- (NSPredicateOperatorType)predicateOperatorType {
    NSPredicateOperatorType type = (NSPredicateOperatorType)_operatorType;
    if (_negate && type == NSEqualToPredicateOperatorType)
        return NSNotEqualToPredicateOperatorType;
    return type;
}

@end

/* The reference's archive classes, read as these. */
Class _NSPredicateArchiveClassForName(NSString *name) {
    static NSDictionary<NSString *, NSString *> *names;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        NSMutableDictionary *map = [NSMutableDictionary dictionary];
        for (NSString *expression in @[ @"NSKeyPathExpression", @"NSSelfExpression", @"NSKeyPathSpecifierExpression",
                                        @"NSConstantValueExpression", @"NSVariableExpression", @"NSFunctionExpression",
                                        @"NSAggregateExpression", @"NSSymbolicExpression" ])
            map[expression] = @"_NSArchivedExpression";
        for (NSString *operator in @[ @"NSPredicateOperator", @"NSEqualityPredicateOperator", @"NSComparisonPredicateOperator",
                                      @"NSLikePredicateOperator", @"NSInPredicateOperator", @"NSStringPredicateOperator",
                                      @"NSMatchingPredicateOperator", @"NSBetweenPredicateOperator",
                                      @"NSSubstringPredicateOperator", @"NSCustomPredicateOperator" ])
            map[operator] = @"_NSArchivedPredicateOperator";
        names = map;
    });
    NSString *replacement = name ? names[name] : nil;
    return replacement ? NSClassFromString(replacement) : Nil;
}
