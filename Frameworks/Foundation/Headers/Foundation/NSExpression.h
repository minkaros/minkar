/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

@class NSArray<ObjectType>, NSMutableDictionary<KeyType, ObjectType>, NSDictionary<KeyType, ObjectType>;

typedef NS_ENUM(NSUInteger, NSExpressionType) {
    NSConstantValueExpressionType = 0,
    NSEvaluatedObjectExpressionType,
    NSVariableExpressionType,
    NSKeyPathExpressionType,
    NSFunctionExpressionType,
    NSUnionSetExpressionType,
    NSIntersectSetExpressionType,
    NSMinusSetExpressionType,
    NSSubqueryExpressionType = 13,
    NSAggregateExpressionType,
    NSAnyKeyExpressionType = 15,
    NSBlockExpressionType = 19,
    NSConditionalExpressionType = 20,
};

@interface NSExpression : NSObject <NSSecureCoding, NSCopying> {
    NSExpressionType _type;
}

- (instancetype)initWithExpressionType:(NSExpressionType)type;

+ (NSExpression *)expressionWithFormat:(NSString *)expressionFormat argumentArray:(NSArray *)arguments;
+ (NSExpression *)expressionWithFormat:(NSString *)expressionFormat, ...;
+ (NSExpression *)expressionWithFormat:(NSString *)expressionFormat arguments:(va_list)argList;

+ (NSExpression *)expressionForConstantValue:value;
+ (NSExpression *)expressionForEvaluatedObject;
+ (NSExpression *)expressionForVariable:(NSString *)string;
+ (NSExpression *)expressionForKeyPath:(NSString *)keyPath;
+ (NSExpression *)expressionForFunction:(NSString *)name arguments:(NSArray *)arguments;

@property (readonly) NSExpressionType expressionType;

- constantValue;
@property (readonly) NSString *variable;
@property (readonly) NSString *keyPath;
@property (readonly) NSString *function;
@property (readonly) NSArray *arguments;
@property (readonly) NSExpression *operand;
@property (readonly, retain) id collection;

- expressionValueWithObject:object context:(NSMutableDictionary *)context;

// private
- (NSExpression *)_expressionWithSubstitutionVariables:(NSDictionary *)variables;

@end
