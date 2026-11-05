/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSData.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSMutableData : NSData

@property (readonly) void *mutableBytes NS_RETURNS_INNER_POINTER;
@property NSUInteger length;

- (void)appendBytes:(const void *)bytes length:(NSUInteger)length;
- (void)appendData:(NSData *)other;
- (void)increaseLengthBy:(NSUInteger)extraLength;
- (void)replaceBytesInRange:(NSRange)range withBytes:(const void *)bytes;
- (void)resetBytesInRange:(NSRange)range;
- (void)setData:(NSData *)data;
- (void)replaceBytesInRange:(NSRange)range withBytes:(nullable const void *)replacementBytes length:(NSUInteger)replacementLength;

+ (nullable instancetype)dataWithCapacity:(NSUInteger)aNumItems;
+ (nullable instancetype)dataWithLength:(NSUInteger)length;
- (nullable instancetype)initWithCapacity:(NSUInteger)capacity;
- (nullable instancetype)initWithLength:(NSUInteger)length;

- (BOOL)decompressUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error;
- (BOOL)compressUsingAlgorithm:(NSDataCompressionAlgorithm)algorithm error:(NSError **)error;

@end

NS_ASSUME_NONNULL_END
