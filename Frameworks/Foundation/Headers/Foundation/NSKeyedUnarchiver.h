/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSCoder.h>
#import <Foundation/NSMapTable.h>

@class NSData, NSDictionary, NSError, NSMutableArray, NSMutableDictionary, NSSet;

typedef NS_ENUM(NSInteger, NSDecodingFailurePolicy) {
    NSDecodingFailurePolicyRaiseException = 0,
    NSDecodingFailurePolicySetErrorAndReturn = 1,
};

FOUNDATION_EXPORT NSString *NSInvalidUnarchiveOperationException;

@interface NSKeyedUnarchiver : NSCoder {
    id _delegate;
    NSMutableDictionary *_nameToReplacementClass;
    NSDictionary *_propertyList;
    NSArray *_objects;
    NSMutableArray *_plistStack;
    NSMapTable *_uidToObject;
    NSMapTable *_objectToUid;
    NSMapTable *_classVersions;

    int _unnamedKeyIndex;
    NSSet *_allowedClasses;
    BOOL _requiresSecureCoding;
    NSInteger _decodingFailurePolicy;
    NSError *_error;
}

// macOS 10.11 to 10.13: secure decoding. With requiresSecureCoding, every
// decoded object's class must be one of the allowed classes (or a
// subclass) and adopt NSSecureCoding.
- (instancetype)initForReadingFromData:(NSData *)data error:(NSError **)error;
+ (id)unarchivedObjectOfClass:(Class)cls fromData:(NSData *)data error:(NSError **)error;
+ (id)unarchivedObjectOfClasses:(NSSet *)classes fromData:(NSData *)data error:(NSError **)error;
+ (id)unarchiveTopLevelObjectWithData:(NSData *)data error:(NSError **)error;
- (id)decodeObjectOfClass:(Class)cls forKey:(NSString *)key;
- (id)decodeObjectOfClasses:(NSSet *)classes forKey:(NSString *)key;
- (id)decodeTopLevelObjectForKey:(NSString *)key error:(NSError **)error;
- (id)decodeTopLevelObjectOfClass:(Class)cls forKey:(NSString *)key error:(NSError **)error;
@property (readwrite) BOOL requiresSecureCoding;
@property (readwrite) NSDecodingFailurePolicy decodingFailurePolicy;
@property (readonly, copy) NSError *error;
@property (readonly, copy) NSSet *allowedClasses;

- initForReadingWithData:(NSData *)data;

+ unarchiveObjectWithData:(NSData *)data;
+ unarchiveObjectWithFile:(NSString *)path;

- (BOOL)containsValueForKey:(NSString *)key;

- (const uint8_t *)decodeBytesForKey:(NSString *)key returnedLength:(NSUInteger *)lengthp;
- (BOOL)decodeBoolForKey:(NSString *)key;
- (double)decodeDoubleForKey:(NSString *)key;
- (float)decodeFloatForKey:(NSString *)key;
- (int)decodeIntForKey:(NSString *)key;
- (int32_t)decodeInt32ForKey:(NSString *)key;
- (int64_t)decodeInt64ForKey:(NSString *)key;
- decodeObjectForKey:(NSString *)key;

- (void)finishDecoding;

- delegate;
- (void)setDelegate:delegate;

+ (void)setClass:(Class)aClass forClassName:(NSString *)className;
+ (Class)classForClassName:(NSString *)className;

- (void)setClass:(Class)aClass forClassName:(NSString *)className;
- (Class)classForClassName:(NSString *)className;

@end

@interface NSObject (NSKeyedUnarchiverDelegate)
- unarchiver:(NSKeyedUnarchiver *)unarchiver didDecodeObject:object;
- (void)unarchiver:(NSKeyedUnarchiver *)unarchiver willReplaceObject:object withObject:replacement;
- (Class)unarchiver:(NSKeyedUnarchiver *)unarchiver cannotDecodeObjectOfClassName:(NSString *)className originalClasses:(NSArray *)classHierarchy;
@end
