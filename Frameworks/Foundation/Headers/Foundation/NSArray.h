/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSEnumerator.h>

@class NSString, NSDictionary, NSPredicate, NSIndexSet, NSURL, HIDEvent;

@interface NSArray <HIDEvent> : NSObject <NSCopying, NSMutableCopying, NSCoding, NSFastEnumeration>

- initWithArray:(NSArray *)array;
- initWithArray:(NSArray *)array copyItems:(BOOL)copyItems;
- initWithContentsOfFile:(NSString *)path;
- initWithContentsOfURL:(NSURL *)url;
- initWithObjects:(id *)objects count:(NSUInteger)count;
- initWithObjects:object, ...;

+ array;
+ arrayWithContentsOfFile:(NSString *)path;
+ arrayWithContentsOfURL:(NSURL *)url;
+ arrayWithObject:object;
+ arrayWithObjects:object, ...;

+ arrayWithArray:(NSArray *)array;
+ arrayWithObjects:(id *)objects count:(NSUInteger)count;

- (NSUInteger)count;
- objectAtIndex:(NSUInteger)index;

- (void)getObjects:(id *)objects;
- (void)getObjects:(id *)objects range:(NSRange)range;

- (NSArray *)objectsAtIndexes:(NSIndexSet *)indexes;

- (NSArray *)subarrayWithRange:(NSRange)range;

- (BOOL)isEqualToArray:(NSArray *)array;

- (NSUInteger)indexOfObject:object;
- (NSUInteger)indexOfObject:object inRange:(NSRange)range;

- (NSUInteger)indexOfObjectIdenticalTo:object;
- (NSUInteger)indexOfObjectIdenticalTo:object inRange:(NSRange)range;

- (NSEnumerator *)objectEnumerator;
- (NSEnumerator *)reverseObjectEnumerator;

- (NSArray *)arrayByAddingObject:object;
- (NSArray *)arrayByAddingObjectsFromArray:(NSArray *)array;
- (NSString *)componentsJoinedByString:(NSString *)string;

- (BOOL)containsObject:object;

- firstObjectCommonWithArray:(NSArray *)array;

- firstObject;
- lastObject;

- (NSArray *)sortedArrayUsingSelector:(SEL)selector;
- (NSArray *)sortedArrayUsingFunction:(NSInteger (*)(id, id, void *))function
                              context:(void *)context;

- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (BOOL)writeToURL:(NSURL *)aURL atomically:(BOOL)atomically;

- (void)makeObjectsPerformSelector:(SEL)selector;
- (void)makeObjectsPerformSelector:(SEL)selector withObject:object;

- (NSString *)descriptionWithLocale:(NSDictionary *)locale;
- (NSString *)descriptionWithLocale:(NSDictionary *)locale indent:(NSUInteger)indent;

- (NSArray *)sortedArrayUsingDescriptors:(NSArray *)descriptors;
- (NSArray *)filteredArrayUsingPredicate:(NSPredicate *)predicate;
@end

typedef NS_OPTIONS(NSUInteger, NSBinarySearchingOptions) {
    NSBinarySearchingFirstEqual = (1UL << 8),
    NSBinarySearchingLastEqual = (1UL << 9),
    NSBinarySearchingInsertionIndex = (1UL << 10),
};

#if __BLOCKS__
@interface NSArray (NSBlocks)
- (void)enumerateObjectsUsingBlock:(void (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))block;
- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))block;
- (void)enumerateObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                       usingBlock:(void (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))block;
- (NSUInteger)indexOfObjectPassingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate;
- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)opts
                           passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate;
- (NSUInteger)indexOfObjectAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                         passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesOfObjectsPassingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesOfObjectsWithOptions:(NSEnumerationOptions)opts
                                passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesOfObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                              passingTest:(BOOL (NS_NOESCAPE ^)(id obj, NSUInteger idx, BOOL *stop))predicate;
- (NSArray *)sortedArrayUsingComparator:(NSComparator NS_NOESCAPE)cmptr;
- (NSArray *)sortedArrayWithOptions:(NSSortOptions)opts usingComparator:(NSComparator NS_NOESCAPE)cmptr;
- (NSUInteger)indexOfObject:(id)obj inSortedRange:(NSRange)r options:(NSBinarySearchingOptions)opts
            usingComparator:(NSComparator NS_NOESCAPE)cmp;
@end
#endif

@interface NSArray (NSSubscripting)
// Subscripting: array[index] (macOS 10.8).
- (id)objectAtIndexedSubscript:(NSUInteger)index;
@end

#import <Foundation/NSMutableArray.h>
