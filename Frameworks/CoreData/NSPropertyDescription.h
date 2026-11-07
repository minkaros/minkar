/* Copyright (c) 2008 Dan Knapp
Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/Foundation.h>
#import <CoreData/CoreDataExports.h>

@class NSEntityDescription;

NS_ASSUME_NONNULL_BEGIN

@interface NSPropertyDescription : NSObject <NSCoding, NSCopying> {
    NSEntityDescription *_entity;
    NSString *_propertyName;
    BOOL _optional;
    BOOL _transient;
    BOOL _indexed;
    BOOL _indexedBySpotlight;
    BOOL _storedInExternalRecord;
    NSDictionary *_userInfo;
    NSArray *_validationPredicates;
    NSArray *_validationWarnings;
    NSString *_versionHashModifier;
    NSString *_renamingIdentifier;
}

@property (nonatomic, readonly, assign) NSEntityDescription *entity;
@property (nonatomic, copy) NSString *name;
@property (getter=isOptional) BOOL optional;
@property (getter=isTransient) BOOL transient;
@property (readonly, copy) NSArray<NSPredicate *> *validationPredicates;
@property (readonly, copy) NSArray *validationWarnings;
@property (nullable, nonatomic, copy) NSDictionary *userInfo;
@property (getter=isIndexed) BOOL indexed API_DEPRECATED("Use NSEntityDescription.indexes instead", macos(10.5, 10.13));
@property (readonly, copy) NSData *versionHash;
@property (nullable, copy) NSString *versionHashModifier;
@property (getter=isIndexedBySpotlight) BOOL indexedBySpotlight;
@property (getter=isStoredInExternalRecord) BOOL storedInExternalRecord API_DEPRECATED("Spotlight integration is deprecated", macos(10.6, 10.13));
@property (nullable, copy) NSString *renamingIdentifier;

- (void)setValidationPredicates:(nullable NSArray<NSPredicate *> *)validationPredicates withValidationWarnings:(nullable NSArray<NSString *> *)validationWarnings;

@end

NS_ASSUME_NONNULL_END
