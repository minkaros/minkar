/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSEnumerator.h>

@class NSArray<ObjectType>, NSURL;

@interface NSDictionary<__covariant KeyType, __covariant ObjectType> : NSObject <NSCoding, NSCopying, NSMutableCopying, NSFastEnumeration>

- initWithObjects:(id *)objects forKeys:(id *)keys count:(NSUInteger)count;
- initWithObjects:(NSArray<ObjectType> *)objects forKeys:(NSArray<KeyType> *)keys;
- initWithDictionary:(NSDictionary<KeyType, ObjectType> *)dictionary;
- initWithDictionary:(NSDictionary<KeyType, ObjectType> *)dictionary copyItems:(BOOL)copyItems;
- initWithObjectsAndKeys:object, ...;
- initWithContentsOfFile:(NSString *)path;
- initWithContentsOfURL:(NSURL *)url;

+ dictionary;
+ dictionaryWithObjects:(id *)objects forKeys:(id *)keys count:(NSUInteger)count;
+ dictionaryWithObjects:(NSArray<ObjectType> *)objects forKeys:(NSArray<KeyType> *)keys;
+ dictionaryWithDictionary:(NSDictionary<KeyType, ObjectType> *)other;
+ dictionaryWithObjectsAndKeys:first, ...;
+ dictionaryWithObject:(ObjectType)object forKey:(KeyType)key;
+ dictionaryWithContentsOfFile:(NSString *)path;
+ dictionaryWithContentsOfURL:(NSURL *)url;

- (ObjectType)objectForKey:(KeyType)key;
- (NSUInteger)count;
- (NSEnumerator<KeyType> *)keyEnumerator;
- (NSEnumerator<ObjectType> *)objectEnumerator;

- (void)getObjects:(id *)objects andKeys:(id *)keys;

- (BOOL)isEqualToDictionary:(NSDictionary<KeyType, ObjectType> *)dictionary;

- (NSArray<KeyType> *)allKeys;
- (NSArray<KeyType> *)allKeysForObject:(ObjectType)object;
- (NSArray<KeyType> *)keysSortedByValueUsingSelector:(SEL)selector;

- (NSArray<ObjectType> *)allValues;
- (NSArray<ObjectType> *)objectsForKeys:(NSArray<KeyType> *)keys notFoundMarker:(ObjectType)marker;

- (BOOL)writeToFile:(NSString *)path atomically:(BOOL)atomically;
- (BOOL)writeToURL:(NSURL *)url atomically:(BOOL)atomically;

- (NSString *)description;
- (NSString *)descriptionInStringsFileFormat;
- (NSString *)descriptionWithLocale:locale;
- (NSString *)descriptionWithLocale:locale indent:(NSUInteger)indent;
@end

#if __BLOCKS__
@class NSSet<ObjectType>;
@interface NSDictionary<KeyType, ObjectType> (NSBlocks)
- (void)enumerateKeysAndObjectsUsingBlock:(void (NS_NOESCAPE ^)(KeyType key, ObjectType obj, BOOL *stop))block;
- (void)enumerateKeysAndObjectsWithOptions:(NSEnumerationOptions)opts
                                usingBlock:(void (NS_NOESCAPE ^)(KeyType key, ObjectType obj, BOOL *stop))block;
- (NSSet<KeyType> *)keysOfEntriesPassingTest:(BOOL (NS_NOESCAPE ^)(KeyType key, ObjectType obj, BOOL *stop))predicate;
- (NSSet<KeyType> *)keysOfEntriesWithOptions:(NSEnumerationOptions)opts
                        passingTest:(BOOL (NS_NOESCAPE ^)(KeyType key, ObjectType obj, BOOL *stop))predicate;
- (NSArray<KeyType> *)keysSortedByValueUsingComparator:(NSComparator NS_NOESCAPE)cmptr;
- (NSArray<KeyType> *)keysSortedByValueWithOptions:(NSSortOptions)opts usingComparator:(NSComparator NS_NOESCAPE)cmptr;
@end
#endif

@interface NSDictionary<KeyType, ObjectType> (NSSubscripting)
// Subscripting: dictionary[key] (macOS 10.8).
- (ObjectType)objectForKeyedSubscript:(KeyType)key;
@end

#import <Foundation/NSMutableDictionary.h>
