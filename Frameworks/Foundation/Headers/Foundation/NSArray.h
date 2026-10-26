/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSRange.h>
#import <Foundation/NSEnumerator.h>

@class NSString, NSPredicate, NSIndexSet, NSURL;
@class NSDictionary<KeyType, ObjectType>;

@interface NSArray<__covariant ObjectType> : NSObject <NSCopying, NSMutableCopying, NSCoding, NSFastEnumeration>

- initWithArray:(NSArray<ObjectType> *)array;
- initWithArray:(NSArray<ObjectType> *)array copyItems:(BOOL)copyItems;
- initWithContentsOfFile:(NSString *)path;
- initWithContentsOfURL:(NSURL *)url;
- initWithObjects:(id *)objects count:(NSUInteger)count;
- initWithObjects:(ObjectType)object, ...;

+ array;
+ arrayWithContentsOfFile:(NSString *)path;
+ arrayWithContentsOfURL:(NSURL *)url;
+ arrayWithObject:(ObjectType)object;
+ arrayWithObjects:(ObjectType)object, ...;

+ arrayWithArray:(NSArray<ObjectType> *)array;
+ arrayWithObjects:(id *)objects count:(NSUInteger)count;

- (NSUInteger)count;
- (ObjectType)objectAtIndex:(NSUInteger)index;

- (void)getObjects:(id *)objects;
- (void)getObjects:(id *)objects range:(NSRange)range;

- (NSArray<ObjectType> *)objectsAtIndexes:(NSIndexSet *)indexes;

- (NSArray<ObjectType> *)subarrayWithRange:(NSRange)range;

- (BOOL)isEqualToArray:(NSArray<ObjectType> *)array;

- (NSUInteger)indexOfObject:(ObjectType)object;
- (NSUInteger)indexOfObject:(ObjectType)object inRange:(NSRange)range;

- (NSUInteger)indexOfObjectIdenticalTo:(ObjectType)object;
- (NSUInteger)indexOfObjectIdenticalTo:(ObjectType)object inRange:(NSRange)range;

- (NSEnumerator<ObjectType> *)objectEnumerator;
- (NSEnumerator<ObjectType> *)reverseObjectEnumerator;

- (NSArray<ObjectType> *)arrayByAddingObject:(ObjectType)object;
- (NSArray<ObjectType> *)arrayByAddingObjectsFromArray:(NSArray<ObjectType> *)array;
- (NSString *)componentsJoinedByString:(NSString *)string;

- (BOOL)containsObject:(ObjectType)object;

- (ObjectType)firstObjectCommonWithArray:(NSArray<ObjectType> *)array;

- (ObjectType)firstObject;
- (ObjectType)lastObject;

- (NSArray<ObjectType> *)sortedArrayUsingSelector:(SEL)selector;
- (NSArray<ObjectType> *)sortedArrayUsingFunction:(NSInteger (*)(ObjectType, ObjectType, void *))function
                              context:(void *)context;

- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (BOOL)writeToURL:(NSURL *)aURL atomically:(BOOL)atomically;

- (void)makeObjectsPerformSelector:(SEL)selector;
- (void)makeObjectsPerformSelector:(SEL)selector withObject:(id)object;

- (NSString *)descriptionWithLocale:(NSDictionary *)locale;
- (NSString *)descriptionWithLocale:(NSDictionary *)locale indent:(NSUInteger)indent;

- (NSArray<ObjectType> *)sortedArrayUsingDescriptors:(NSArray *)descriptors;
- (NSArray<ObjectType> *)filteredArrayUsingPredicate:(NSPredicate *)predicate;
@end

typedef NS_OPTIONS(NSUInteger, NSBinarySearchingOptions) {
    NSBinarySearchingFirstEqual = (1UL << 8),
    NSBinarySearchingLastEqual = (1UL << 9),
    NSBinarySearchingInsertionIndex = (1UL << 10),
};

#if __BLOCKS__
@interface NSArray<ObjectType> (NSBlocks)
- (void)enumerateObjectsUsingBlock:(void (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))block;
- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))block;
- (void)enumerateObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                       usingBlock:(void (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))block;
- (NSUInteger)indexOfObjectPassingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))predicate;
- (NSUInteger)indexOfObjectWithOptions:(NSEnumerationOptions)opts
                           passingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))predicate;
- (NSUInteger)indexOfObjectAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                         passingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesOfObjectsPassingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesOfObjectsWithOptions:(NSEnumerationOptions)opts
                                passingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))predicate;
- (NSIndexSet *)indexesOfObjectsAtIndexes:(NSIndexSet *)indexes options:(NSEnumerationOptions)opts
                              passingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, NSUInteger idx, BOOL *stop))predicate;
- (NSArray<ObjectType> *)sortedArrayUsingComparator:(NSComparator NS_NOESCAPE)cmptr;
- (NSArray<ObjectType> *)sortedArrayWithOptions:(NSSortOptions)opts usingComparator:(NSComparator NS_NOESCAPE)cmptr;
- (NSUInteger)indexOfObject:(ObjectType)obj inSortedRange:(NSRange)r options:(NSBinarySearchingOptions)opts
            usingComparator:(NSComparator NS_NOESCAPE)cmp;
@end
#endif

@interface NSArray<ObjectType> (NSSubscripting)
// Subscripting: array[index] (macOS 10.8).
- (ObjectType)objectAtIndexedSubscript:(NSUInteger)index;
@end

#import <Foundation/NSMutableArray.h>
