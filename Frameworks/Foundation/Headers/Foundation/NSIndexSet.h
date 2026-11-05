/* Copyright (c) 2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSIndexSet : NSObject <NSCoding, NSCopying, NSMutableCopying> {
    NSUInteger _length;
    NSRange *_ranges;
}

+ (instancetype)indexSetWithIndexesInRange:(NSRange)range;
+ (instancetype)indexSetWithIndex:(NSUInteger)index;
+ (instancetype)indexSet;

- (instancetype)initWithIndexSet:(NSIndexSet *)other;
- (instancetype)initWithIndexesInRange:(NSRange)range;
- (instancetype)initWithIndex:(NSUInteger)index;
- (nonnull instancetype)init;

- (BOOL)isEqualToIndexSet:(NSIndexSet *)other;

@property (readonly) NSUInteger count;
@property (readonly) NSUInteger firstIndex;
@property (readonly) NSUInteger lastIndex;
- (NSUInteger)getIndexes:(NSUInteger *)buffer maxCount:(NSUInteger)capacity inIndexRange:(nullable NSRangePointer)range;

- (BOOL)containsIndexesInRange:(NSRange)range;
- (BOOL)containsIndexes:(NSIndexSet *)other;
- (BOOL)containsIndex:(NSUInteger)index;

- (NSUInteger)indexGreaterThanIndex:(NSUInteger)index;
- (NSUInteger)indexGreaterThanOrEqualToIndex:(NSUInteger)index;
- (NSUInteger)indexLessThanIndex:(NSUInteger)index;
- (NSUInteger)indexLessThanOrEqualToIndex:(NSUInteger)index;

- (BOOL)intersectsIndexesInRange:(NSRange)range;

- (void)encodeWithCoder:(NSCoder *)encoder;
- (nullable id)initWithCoder:(NSCoder *)decoder;

@end

#if __BLOCKS__
@interface NSIndexSet (NSBlocks)
- (void)enumerateIndexesUsingBlock:(void (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))block;
- (void)enumerateIndexesWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))block;
- (void)enumerateRangesUsingBlock:(void (NS_NOESCAPE ^)(NSRange range, BOOL *stop))block;
- (NSUInteger)indexPassingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesPassingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate;
- (NSUInteger)countOfIndexesInRange:(NSRange)range;
- (void)enumerateIndexesInRange:(NSRange)range options:(NSEnumerationOptions)opts usingBlock:(void (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))block;
- (void)enumerateRangesWithOptions:(NSEnumerationOptions)opts usingBlock:(void (NS_NOESCAPE ^)(NSRange range, BOOL *stop))block;
- (void)enumerateRangesInRange:(NSRange)range options:(NSEnumerationOptions)opts usingBlock:(void (NS_NOESCAPE ^)(NSRange range, BOOL *stop))block;
- (NSUInteger)indexWithOptions:(NSEnumerationOptions)opts passingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate;
- (NSUInteger)indexInRange:(NSRange)range options:(NSEnumerationOptions)opts passingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesWithOptions:(NSEnumerationOptions)opts passingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesInRange:(NSRange)range options:(NSEnumerationOptions)opts passingTest:(BOOL (NS_NOESCAPE ^)(NSUInteger idx, BOOL *stop))predicate;
@end

NS_ASSUME_NONNULL_END
#endif

#import <Foundation/NSMutableIndexSet.h>
