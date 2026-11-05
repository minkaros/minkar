/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSValue.h>
#import <Foundation/NSDecimal.h>

@class NSDictionary<KeyType, ObjectType>;

NS_ASSUME_NONNULL_BEGIN

@interface NSNumber : NSValue

- (NSNumber *)initWithBool:(BOOL)value;
- (NSNumber *)initWithChar:(char)value;
- (NSNumber *)initWithShort:(short)value;
- (NSNumber *)initWithInt:(int)value;
- (NSNumber *)initWithLong:(long)value;
- (NSNumber *)initWithLongLong:(long long)value;
- (NSNumber *)initWithFloat:(float)value;
- (NSNumber *)initWithDouble:(double)value;
- (NSNumber *)initWithInteger:(NSInteger)value;

- (NSNumber *)initWithUnsignedChar:(unsigned char)value;
- (NSNumber *)initWithUnsignedShort:(unsigned short)value;
- (NSNumber *)initWithUnsignedInt:(unsigned int)value;
- (NSNumber *)initWithUnsignedLong:(unsigned long)value;
- (NSNumber *)initWithUnsignedLongLong:(unsigned long long)value;
- (NSNumber *)initWithUnsignedInteger:(NSUInteger)value;

+ (NSNumber *)numberWithBool:(BOOL)value;
+ (NSNumber *)numberWithChar:(char)value;
+ (NSNumber *)numberWithShort:(short)value;
+ (NSNumber *)numberWithInt:(int)value;
+ (NSNumber *)numberWithLong:(long)value;
+ (NSNumber *)numberWithLongLong:(long long)value;
+ (NSNumber *)numberWithFloat:(float)value;
+ (NSNumber *)numberWithDouble:(double)value;
+ (NSNumber *)numberWithInteger:(NSInteger)value;

+ (NSNumber *)numberWithUnsignedChar:(unsigned char)value;
+ (NSNumber *)numberWithUnsignedShort:(unsigned short)value;
+ (NSNumber *)numberWithUnsignedInt:(unsigned int)value;
+ (NSNumber *)numberWithUnsignedLong:(unsigned long)value;
+ (NSNumber *)numberWithUnsignedLongLong:(unsigned long long)value;
+ (NSNumber *)numberWithUnsignedInteger:(NSUInteger)value;

- (NSComparisonResult)compare:(NSNumber *)other;

- (BOOL)isEqualToNumber:(NSNumber *)other;

@property (readonly) char charValue;
@property (readonly) short shortValue;
@property (readonly) int intValue;
@property (readonly) long longValue;
@property (readonly) long long longLongValue;
@property (readonly) float floatValue;
@property (readonly) double doubleValue;
@property (readonly) BOOL boolValue;
@property (readonly) NSDecimal decimalValue;
@property (readonly) NSInteger integerValue;
@property (readonly) unsigned char unsignedCharValue;
@property (readonly) unsigned short unsignedShortValue;
@property (readonly) unsigned int unsignedIntValue;
@property (readonly) unsigned long unsignedLongValue;
@property (readonly) unsigned long long unsignedLongLongValue;
@property (readonly) NSUInteger unsignedIntegerValue;

@property (readonly, copy) NSString *stringValue;

- (NSString *)descriptionWithLocale:(nullable id)locale;

@end

NS_ASSUME_NONNULL_END
