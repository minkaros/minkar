/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

@class NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>;

NS_ASSUME_NONNULL_BEGIN

@interface NSPredicate : NSObject <NSSecureCoding, NSCopying> {
    NSString *_format;
    NSPredicate *_predicate;
}

+ (NSPredicate *)predicateWithFormat:(NSString *)format arguments:(va_list)arguments;
+ (NSPredicate *)predicateWithFormat:(NSString *)format, ...;
+ (NSPredicate *)predicateWithFormat:(NSString *)format argumentArray:(nullable NSArray *)arguments;
+ (NSPredicate *)predicateWithValue:(BOOL)value;
+ (NSPredicate *)predicateWithBlock:(BOOL (^)(id _Nullable evaluatedObject, NSDictionary<NSString *, id> * _Nullable bindings))block;

@property (readonly, copy) NSString *predicateFormat;

- (instancetype)predicateWithSubstitutionVariables:(NSDictionary<NSString *, id> *)variables;

- (BOOL)evaluateWithObject:(nullable id)object;
- (BOOL)evaluateWithObject:(nullable id)object substitutionVariables:(nullable NSDictionary<NSString *, id> *)bindings;

- (void)allowEvaluation;

@end

NS_ASSUME_NONNULL_END
