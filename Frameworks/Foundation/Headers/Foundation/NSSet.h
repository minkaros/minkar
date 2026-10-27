/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSEnumerator.h>

@class NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSString, NSPredicate;

@interface NSSet<__covariant ObjectType> : NSObject <NSCoding, NSCopying, NSMutableCopying, NSFastEnumeration>

- initWithObjects:(id *)objects count:(NSUInteger)count;
- initWithArray:(NSArray<ObjectType> *)array;
- initWithSet:(NSSet<ObjectType> *)set;
- initWithSet:(NSSet<ObjectType> *)set copyItems:(BOOL)copyItems;
- initWithObjects:(ObjectType)first, ...;

+ set;
+ setWithArray:(NSArray<ObjectType> *)array;
+ setWithSet:(NSSet<ObjectType> *)set;
+ setWithObject:(ObjectType)object;
+ setWithObjects:(ObjectType)first, ...;
+ setWithObjects:(id *)objects count:(NSUInteger)count;

- (NSSet<ObjectType> *)setByAddingObject:(ObjectType)object;
- (NSSet<ObjectType> *)setByAddingObjectsFromSet:(NSSet<ObjectType> *)other;
- (NSSet<ObjectType> *)setByAddingObjectsFromArray:(NSArray<ObjectType> *)array;

- (ObjectType)member:(ObjectType)object;
- (NSUInteger)count;
- (NSEnumerator<ObjectType> *)objectEnumerator;

- (BOOL)isEqualToSet:(NSSet<ObjectType> *)set;

- (NSArray<ObjectType> *)allObjects;

- (BOOL)containsObject:(ObjectType)object;
- (BOOL)isSubsetOfSet:(NSSet<ObjectType> *)set;

- (BOOL)intersectsSet:(NSSet<ObjectType> *)set;

- (void)makeObjectsPerformSelector:(SEL)selector;
- (void)makeObjectsPerformSelector:(SEL)selector withObject:(id)argument;

- (ObjectType)anyObject;

- (NSString *)descriptionWithLocale:(NSDictionary *)locale;

- (NSSet<ObjectType> *)filteredSetUsingPredicate:(NSPredicate *)predicate;

@end

#if __BLOCKS__
@interface NSSet<ObjectType> (NSBlocks)
- (void)enumerateObjectsUsingBlock:(void (NS_NOESCAPE ^)(ObjectType obj, BOOL *stop))block;
- (void)enumerateObjectsWithOptions:(NSEnumerationOptions)opts
                         usingBlock:(void (NS_NOESCAPE ^)(ObjectType obj, BOOL *stop))block;
- (NSSet<ObjectType> *)objectsPassingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, BOOL *stop))predicate;
- (NSSet<ObjectType> *)objectsWithOptions:(NSEnumerationOptions)opts
                  passingTest:(BOOL (NS_NOESCAPE ^)(ObjectType obj, BOOL *stop))predicate;
@end
#endif

#import <Foundation/NSMutableSet.h>
