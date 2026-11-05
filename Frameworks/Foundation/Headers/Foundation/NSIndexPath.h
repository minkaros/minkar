/* Copyright (c) 2007 Dirk Theisen

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSIndexPath : NSObject <NSCopying, NSSecureCoding> {
    NSUInteger _length;
    NSUInteger *_indexes;
}

+ (instancetype)indexPathWithIndex:(NSUInteger)index;
+ (instancetype)indexPathWithIndexes:(const NSUInteger [_Nullable])indexes length:(NSUInteger)length;

- (instancetype)initWithIndexes:(const NSUInteger [_Nullable])indexes length:(NSUInteger)length NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithIndex:(NSUInteger)index;

- (NSIndexPath *)indexPathByAddingIndex:(NSUInteger)index;
- (NSIndexPath *)indexPathByRemovingLastIndex;

- (NSUInteger)indexAtPosition:(NSUInteger)position;
@property (readonly) NSUInteger length;

- (void)getIndexes:(NSUInteger *)indexes range:(NSRange)positionRange;
- (void)getIndexes:(NSUInteger *)indexes;

- (NSComparisonResult)compare:(NSIndexPath *)otherObject;

@end

NS_ASSUME_NONNULL_END
