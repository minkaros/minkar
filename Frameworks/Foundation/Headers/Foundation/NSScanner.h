/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSDecimal.h>

@class NSDictionary<KeyType, ObjectType>, NSCharacterSet, NSString;

NS_ASSUME_NONNULL_BEGIN

@interface NSScanner : NSObject <NSCopying>

+ (instancetype)scannerWithString:(NSString *)string;
+ (id)localizedScannerWithString:(NSString *)string;

- (instancetype)initWithString:(NSString *)string;

@property (readonly, copy) NSString *string;
@property NSUInteger scanLocation;
@property (nullable, copy) NSCharacterSet *charactersToBeSkipped;
@property BOOL caseSensitive;
@property (nullable, retain) id locale;

@property (readonly, getter=isAtEnd) BOOL atEnd;

- (BOOL)scanInt:(nullable int *)result;
- (BOOL)scanInteger:(nullable NSInteger *)result;
- (BOOL)scanLongLong:(nullable long long *)result;
- (BOOL)scanUnsignedLongLong:(nullable unsigned long long *)result;
- (BOOL)scanFloat:(nullable float *)result;
- (BOOL)scanDouble:(nullable double *)result;
- (BOOL)scanDecimal:(nullable NSDecimal *)dcm;
- (BOOL)scanHexInt:(nullable unsigned *)result;
- (BOOL)scanHexLongLong:(nullable unsigned long long *)result;
- (BOOL)scanHexFloat:(nullable float *)result;
- (BOOL)scanHexDouble:(nullable double *)result;

- (BOOL)scanString:(NSString *)string intoString:(NSString * _Nullable * _Nullable)result;
- (BOOL)scanUpToString:(NSString *)string intoString:(NSString * _Nullable * _Nullable)result;

- (BOOL)scanCharactersFromSet:(NSCharacterSet *)set intoString:(NSString * _Nullable * _Nullable)result;
- (BOOL)scanUpToCharactersFromSet:(NSCharacterSet *)set intoString:(NSString * _Nullable * _Nullable)result;

@end

NS_ASSUME_NONNULL_END
