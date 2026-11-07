/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Items offered in representations by type (API 19): drag and drop, the
 * pasteboard, sharing.
 */

#import <Foundation/NSObject.h>

@class NSArray<ObjectType>, NSMutableArray<ObjectType>, NSData, NSDictionary<KeyType, ObjectType>, NSError, NSProgress, NSString, NSURL;

NS_ASSUME_NONNULL_BEGIN

typedef NS_ENUM(NSInteger, NSItemProviderRepresentationVisibility) {
    NSItemProviderRepresentationVisibilityAll = 0,
    NSItemProviderRepresentationVisibilityTeam = 1,
    NSItemProviderRepresentationVisibilityGroup = 2,
    NSItemProviderRepresentationVisibilityOwnProcess = 3,
};

typedef NS_OPTIONS(NSInteger, NSItemProviderFileOptions) {
    NSItemProviderFileOptionOpenInPlace = 1,
};

@protocol NSItemProviderWriting <NSObject>
@property (class, readonly, copy) NSArray<NSString *> *writableTypeIdentifiersForItemProvider;
@optional
@property (readonly, copy) NSArray<NSString *> *writableTypeIdentifiersForItemProvider;
+ (NSItemProviderRepresentationVisibility)itemProviderVisibilityForRepresentationWithTypeIdentifier:(NSString *)typeIdentifier;
- (NSItemProviderRepresentationVisibility)itemProviderVisibilityForRepresentationWithTypeIdentifier:(NSString *)typeIdentifier;
@required
- (nullable NSProgress *)loadDataWithTypeIdentifier:(NSString *)typeIdentifier forItemProviderCompletionHandler:(void (^)(NSData * _Nullable data, NSError * _Nullable error))completionHandler;
@end

@protocol NSItemProviderReading <NSObject>
@property (class, readonly, copy) NSArray<NSString *> *readableTypeIdentifiersForItemProvider;
+ (nullable instancetype)objectWithItemProviderData:(NSData *)data typeIdentifier:(NSString *)typeIdentifier error:(NSError **)outError;
@end

typedef void (^NSItemProviderCompletionHandler)(__nullable id <NSSecureCoding> item, NSError * __null_unspecified error);
typedef void (^NSItemProviderLoadHandler)(__null_unspecified NSItemProviderCompletionHandler completionHandler, __null_unspecified Class expectedValueClass, NSDictionary * __null_unspecified options);

@interface NSItemProvider : NSObject <NSCopying> {
    NSMutableArray *_representations;
}

- (instancetype)init NS_DESIGNATED_INITIALIZER;

- (void)registerDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier visibility:(NSItemProviderRepresentationVisibility)visibility loadHandler:(NSProgress * _Nullable (^)(void (^completionHandler)(NSData * _Nullable data, NSError * _Nullable error)))loadHandler;
- (void)registerFileRepresentationForTypeIdentifier:(NSString *)typeIdentifier fileOptions:(NSItemProviderFileOptions)fileOptions visibility:(NSItemProviderRepresentationVisibility)visibility loadHandler:(NSProgress * _Nullable (^)(void (^completionHandler)(NSURL * _Nullable url, BOOL coordinated, NSError * _Nullable error)))loadHandler;

@property (copy, readonly) NSArray<NSString *> *registeredTypeIdentifiers;
- (NSArray<NSString *> *)registeredTypeIdentifiersWithFileOptions:(NSItemProviderFileOptions)fileOptions;
- (BOOL)hasItemConformingToTypeIdentifier:(NSString *)typeIdentifier;
- (BOOL)hasRepresentationConformingToTypeIdentifier:(NSString *)typeIdentifier fileOptions:(NSItemProviderFileOptions)fileOptions;

- (NSProgress *)loadDataRepresentationForTypeIdentifier:(NSString *)typeIdentifier completionHandler:(void (^)(NSData * _Nullable data, NSError * _Nullable error))completionHandler;
- (NSProgress *)loadFileRepresentationForTypeIdentifier:(NSString *)typeIdentifier completionHandler:(void (^)(NSURL * _Nullable url, NSError * _Nullable error))completionHandler;
- (nullable NSProgress *)loadInPlaceFileRepresentationForTypeIdentifier:(NSString *)typeIdentifier completionHandler:(void (^)(NSURL * _Nullable url, BOOL isInPlace, NSError * _Nullable error))completionHandler;

@property (nullable, copy) NSString *suggestedName;

- (instancetype)initWithObject:(id <NSItemProviderWriting>)object;
- (void)registerObject:(id <NSItemProviderWriting>)object visibility:(NSItemProviderRepresentationVisibility)visibility;
- (void)registerObjectOfClass:(Class<NSItemProviderWriting>)aClass visibility:(NSItemProviderRepresentationVisibility)visibility loadHandler:(NSProgress * _Nullable (^)(void (^completionHandler)(id <NSItemProviderWriting> _Nullable object, NSError * _Nullable error)))loadHandler;
- (BOOL)canLoadObjectOfClass:(Class<NSItemProviderReading>)aClass;
- (NSProgress *)loadObjectOfClass:(Class<NSItemProviderReading>)aClass completionHandler:(void (^)(id <NSItemProviderReading> _Nullable object, NSError * _Nullable error))completionHandler;

- (instancetype)initWithItem:(nullable id <NSSecureCoding>)item typeIdentifier:(nullable NSString *)typeIdentifier NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithContentsOfURL:(null_unspecified NSURL *)fileURL;
- (void)registerItemForTypeIdentifier:(NSString *)typeIdentifier loadHandler:(NSItemProviderLoadHandler)loadHandler;
- (void)loadItemForTypeIdentifier:(NSString *)typeIdentifier options:(nullable NSDictionary *)options completionHandler:(nullable NSItemProviderCompletionHandler)completionHandler;

@end

FOUNDATION_EXPORT NSString * const NSItemProviderPreferredImageSizeKey;
FOUNDATION_EXPORT NSString * const NSItemProviderErrorDomain;

typedef NS_ENUM(NSInteger, NSItemProviderErrorCode) {
    NSItemProviderUnknownError = -1,
    NSItemProviderItemUnavailableError = -1000,
    NSItemProviderUnexpectedValueClassError = -1100,
    NSItemProviderUnavailableCoercionError = -1200
};

NS_ASSUME_NONNULL_END
