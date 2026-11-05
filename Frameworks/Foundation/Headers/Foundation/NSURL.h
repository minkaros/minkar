/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>

@class NSURLHandle, NSNumber, NSData, NSError;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSURLFileScheme;

@interface NSURL : NSObject <NSCopying, NSCoding> {
    NSURL *_baseURL;
    NSString *_string;
    NSString *_scheme;
    NSString *_host;
    NSNumber *_port;
    NSString *_user;
    NSString *_password;
    NSString *_path;
    NSString *_parameter;
    NSString *_query;
    NSString *_fragment;
}

- (nullable instancetype)initWithScheme:(NSString *)scheme host:(nullable NSString *)host path:(NSString *)path;
- (instancetype)initFileURLWithPath:(NSString *)path;
- (nullable instancetype)initWithString:(NSString *)string;
- (nullable instancetype)initWithString:(NSString *)string relativeToURL:(nullable NSURL *)parent;
- (instancetype)initWithDataRepresentation:(NSData *)data relativeToURL:(nullable NSURL *)baseURL;
- (instancetype)initAbsoluteURLWithDataRepresentation:(NSData *)data relativeToURL:(nullable NSURL *)baseURL;

+ (NSURL *)fileURLWithPath:(NSString *)path;
+ (nullable instancetype)URLWithString:(NSString *)string;
+ (nullable instancetype)URLWithString:(NSString *)string relativeToURL:(nullable NSURL *)parent;
+ (NSURL *)URLWithDataRepresentation:(NSData *)data relativeToURL:(nullable NSURL *)baseURL;
+ (NSURL *)absoluteURLWithDataRepresentation:(NSData *)data relativeToURL:(nullable NSURL *)baseURL;

@property (readonly, copy) NSData *dataRepresentation;

@property (nullable, readonly, copy) NSString *absoluteString;
@property (nullable, readonly, copy) NSString *parameterString;
- propertyForKey:(NSString *)key;

@property (nullable, readonly, copy) NSString *scheme;
@property (nullable, readonly, copy) NSString *host;
@property (nullable, readonly, copy) NSString *user;
@property (nullable, readonly, copy) NSString *password;
@property (nullable, readonly, copy) NSString *fragment;
@property (nullable, readonly, copy) NSString *path;
@property (nullable, readonly, copy) NSNumber *port;
@property (nullable, readonly, copy) NSString *query;
@property (nullable, readonly, copy) NSString *relativePath;
@property (readonly, copy) NSString *relativeString;
@property (nullable, readonly, copy) NSString *resourceSpecifier;

@property (readonly, getter=isFileURL) BOOL fileURL;

@property (nullable, readonly, copy) NSURL *standardizedURL;
@property (nullable, readonly, copy) NSURL *absoluteURL;
@property (nullable, readonly, copy) NSURL *baseURL;

- (nullable NSURL *)URLByAppendingPathComponent:(NSString *)pathComponent;
@property (nullable, readonly, copy) NSURL *URLByDeletingLastPathComponent;
@property (nullable, readonly, copy) NSString *lastPathComponent;
@property (nullable, readonly, copy) NSString *pathExtension;

- (BOOL)setProperty:(id)property forKey:(NSString *)key;

- (BOOL)setResourceData:(NSData *)data;

- (nullable NSData *)resourceDataUsingCache:(BOOL)useCache;
- (nullable NSURLHandle *)URLHandleUsingCache:(BOOL)useCache;
- (void)loadResourceDataNotifyingClient:(id)client usingCache:(BOOL)useCache;

@end

typedef NSString *NSURLResourceKey NS_TYPED_EXTENSIBLE_ENUM;
FOUNDATION_EXPORT NSURLResourceKey const NSURLNameKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLLocalizedNameKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsRegularFileKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsDirectoryKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsSymbolicLinkKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsVolumeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsPackageKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsApplicationKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsSystemImmutableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsUserImmutableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsHiddenKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLHasHiddenExtensionKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLCreationDateKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLContentAccessDateKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLContentModificationDateKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLAttributeModificationDateKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLLinkCountKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLParentDirectoryURLKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeURLKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLTypeIdentifierKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLLocalizedTypeDescriptionKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLLabelNumberKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLLabelColorKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLLocalizedLabelKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLEffectiveIconKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLCustomIconKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLFileResourceIdentifierKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIdentifierKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLPreferredIOBlockSizeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsReadableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsWritableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsExecutableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLFileSecurityKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsExcludedFromBackupKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLTagNamesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLPathKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLCanonicalPathKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsMountTriggerKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLGenerationIdentifierKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLDocumentIdentifierKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLAddedToDirectoryDateKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLQuarantinePropertiesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLFileResourceTypeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLThumbnailDictionaryKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLApplicationIsScriptableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsAliasFileKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLFileSizeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLFileAllocatedSizeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLTotalFileSizeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLTotalFileAllocatedSizeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeLocalizedFormatDescriptionKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeTotalCapacityKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeAvailableCapacityKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeResourceCountKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsPersistentIDsKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsSymbolicLinksKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsHardLinksKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsJournalingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsJournalingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsSparseFilesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsZeroRunsKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsCaseSensitiveNamesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsCasePreservedNamesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsRootDirectoryDatesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsVolumeSizesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsRenamingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsAdvisoryFileLockingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsExtendedSecurityKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsBrowsableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeMaximumFileSizeKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsEjectableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsRemovableKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsInternalKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsAutomountedKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsLocalKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsReadOnlyKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeCreationDateKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeURLForRemountingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeUUIDStringKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeNameKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeLocalizedNameKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsEncryptedKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeIsRootFileSystemKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsCompressionKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsFileCloningKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsSwapRenamingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsExclusiveRenamingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsImmutableFilesKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeSupportsAccessPermissionsKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeAvailableCapacityForImportantUsageKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLVolumeAvailableCapacityForOpportunisticUsageKey;

/* iCloud's keys: no item is ubiquitous on Minkar. */
FOUNDATION_EXPORT NSURLResourceKey const NSURLIsUbiquitousItemKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemHasUnresolvedConflictsKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemIsDownloadingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemIsUploadedKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemIsUploadingKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemDownloadingStatusKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemDownloadingErrorKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemUploadingErrorKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemDownloadRequestedKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemContainerDisplayNameKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousItemIsSharedKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousSharedItemCurrentUserRoleKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousSharedItemCurrentUserPermissionsKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousSharedItemOwnerNameComponentsKey;
FOUNDATION_EXPORT NSURLResourceKey const NSURLUbiquitousSharedItemMostRecentEditorNameComponentsKey;

typedef NSString *NSURLUbiquitousItemDownloadingStatus NS_TYPED_ENUM;
FOUNDATION_EXPORT NSURLUbiquitousItemDownloadingStatus const NSURLUbiquitousItemDownloadingStatusNotDownloaded;
FOUNDATION_EXPORT NSURLUbiquitousItemDownloadingStatus const NSURLUbiquitousItemDownloadingStatusDownloaded;
FOUNDATION_EXPORT NSURLUbiquitousItemDownloadingStatus const NSURLUbiquitousItemDownloadingStatusCurrent;
typedef NSString *NSURLUbiquitousSharedItemRole NS_TYPED_ENUM NS_SWIFT_NAME(URLUbiquitousSharedItemRole);
FOUNDATION_EXPORT NSURLUbiquitousSharedItemRole const NSURLUbiquitousSharedItemRoleOwner;
FOUNDATION_EXPORT NSURLUbiquitousSharedItemRole const NSURLUbiquitousSharedItemRoleParticipant;
typedef NSString *NSURLUbiquitousSharedItemPermissions NS_TYPED_ENUM NS_SWIFT_NAME(URLUbiquitousSharedItemPermissions);
FOUNDATION_EXPORT NSURLUbiquitousSharedItemPermissions const NSURLUbiquitousSharedItemPermissionsReadOnly;
FOUNDATION_EXPORT NSURLUbiquitousSharedItemPermissions const NSURLUbiquitousSharedItemPermissionsReadWrite;

/* A file's owner, group and access control list (NSURLFileSecurityKey). */
@interface NSFileSecurity : NSObject <NSCopying, NSSecureCoding>
- (nullable instancetype)initWithCoder:(NSCoder *)coder NS_DESIGNATED_INITIALIZER;
@end

typedef NSString *NSURLFileResourceType NS_TYPED_ENUM;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeNamedPipe;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeCharacterSpecial;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeDirectory;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeBlockSpecial;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeRegular;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeSymbolicLink;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeSocket;
FOUNDATION_EXPORT NSURLFileResourceType const NSURLFileResourceTypeUnknown;

typedef NS_OPTIONS(NSUInteger, NSURLBookmarkCreationOptions) {
    NSURLBookmarkCreationPreferFileIDResolution = (1UL << 8),
    NSURLBookmarkCreationMinimalBookmark = (1UL << 9),
    NSURLBookmarkCreationSuitableForBookmarkFile = (1UL << 10),
    NSURLBookmarkCreationWithSecurityScope = (1UL << 11),
    NSURLBookmarkCreationSecurityScopeAllowOnlyReadAccess = (1UL << 12),
};

typedef NS_OPTIONS(NSUInteger, NSURLBookmarkResolutionOptions) {
    NSURLBookmarkResolutionWithoutUI = (1UL << 8),
    NSURLBookmarkResolutionWithoutMounting = (1UL << 9),
    NSURLBookmarkResolutionWithSecurityScope = (1UL << 10),
};

typedef NSUInteger NSURLBookmarkFileCreationOptions;

@interface NSURL (NSURLResources)

+ (NSURL *)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir;
+ (NSURL *)fileURLWithPath:(NSString *)path relativeToURL:(nullable NSURL *)baseURL;
+ (NSURL *)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir relativeToURL:(nullable NSURL *)baseURL;
+ (NSURL *)fileURLWithFileSystemRepresentation:(const char *)path isDirectory:(BOOL)isDir relativeToURL:(nullable NSURL *)baseURL;
- (instancetype)initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir;
- (instancetype)initFileURLWithPath:(NSString *)path relativeToURL:(nullable NSURL *)baseURL;
- (instancetype)initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir relativeToURL:(nullable NSURL *)baseURL;
- (instancetype)initFileURLWithFileSystemRepresentation:(const char *)path isDirectory:(BOOL)isDir relativeToURL:(nullable NSURL *)baseURL;

- (BOOL)getFileSystemRepresentation:(char *)buffer maxLength:(NSUInteger)maxBufferLength;
@property (readonly) const char *fileSystemRepresentation;
@property (readonly) BOOL hasDirectoryPath;
- (BOOL)isFileReferenceURL;
- (nullable NSURL *)fileReferenceURL;
@property (nullable, readonly, copy) NSURL *filePathURL;

- (BOOL)getResourceValue:(out id _Nullable * _Nonnull)value forKey:(NSURLResourceKey)key error:(out NSError **)error NS_REFINED_FOR_SWIFT;
- (nullable NSDictionary<NSURLResourceKey, id> *)resourceValuesForKeys:(NSArray<NSURLResourceKey> *)keys error:(NSError **)error;
- (BOOL)setResourceValue:(nullable id)value forKey:(NSURLResourceKey)key error:(NSError **)error;
- (BOOL)setResourceValues:(NSDictionary<NSURLResourceKey, id> *)keyedValues error:(NSError **)error;
- (void)removeCachedResourceValueForKey:(NSURLResourceKey)key;
- (void)removeAllCachedResourceValues;
- (void)setTemporaryResourceValue:(nullable id)value forKey:(NSURLResourceKey)key;
- (BOOL)checkResourceIsReachableAndReturnError:(NSError **)error NS_SWIFT_NOTHROW;

- (nullable NSData *)bookmarkDataWithOptions:(NSURLBookmarkCreationOptions)options
     includingResourceValuesForKeys:(nullable NSArray<NSURLResourceKey> *)keys
                      relativeToURL:(nullable NSURL *)relativeURL error:(NSError **)error;
- (nullable instancetype)initByResolvingBookmarkData:(NSData *)bookmarkData options:(NSURLBookmarkResolutionOptions)options
                              relativeToURL:(nullable NSURL *)relativeURL bookmarkDataIsStale:(nullable BOOL *)isStale
                                      error:(NSError **)error;
+ (nullable instancetype)URLByResolvingBookmarkData:(NSData *)bookmarkData options:(NSURLBookmarkResolutionOptions)options
                             relativeToURL:(nullable NSURL *)relativeURL bookmarkDataIsStale:(nullable BOOL *)isStale
                                     error:(NSError **)error;
+ (nullable NSDictionary<NSURLResourceKey, id> *)resourceValuesForKeys:(NSArray<NSURLResourceKey> *)keys
                                             fromBookmarkData:(NSData *)bookmarkData;
+ (BOOL)writeBookmarkData:(NSData *)bookmarkData toURL:(NSURL *)bookmarkFileURL
                  options:(NSURLBookmarkFileCreationOptions)options error:(NSError **)error;
+ (nullable NSData *)bookmarkDataWithContentsOfURL:(NSURL *)bookmarkFileURL error:(NSError **)error;
+ (nullable instancetype)URLByResolvingAliasFileAtURL:(NSURL *)url options:(NSURLBookmarkResolutionOptions)options
                                       error:(NSError **)error;
- (BOOL)startAccessingSecurityScopedResource;
- (void)stopAccessingSecurityScopedResource;

@end

/* Items promised by a file provider: none on Minkar, so the item itself. */
@interface NSURL (NSPromisedItems)
- (BOOL)getPromisedItemResourceValue:(id _Nullable * _Nonnull)value forKey:(NSURLResourceKey)key error:(NSError **)error NS_REFINED_FOR_SWIFT;
- (nullable NSDictionary<NSURLResourceKey, id> *)promisedItemResourceValuesForKeys:(NSArray<NSURLResourceKey> *)keys error:(NSError **)error;
- (BOOL)checkPromisedItemIsReachableAndReturnError:(NSError **)error NS_SWIFT_NOTHROW;
@end

@interface NSURL (NSURLPathUtilities)

+ (nullable NSURL *)fileURLWithPathComponents:(NSArray<NSString *> *)components;
@property (nullable, readonly, copy) NSArray<NSString *> *pathComponents;
- (nullable NSURL *)URLByAppendingPathComponent:(NSString *)pathComponent isDirectory:(BOOL)isDirectory;
- (nullable NSURL *)URLByAppendingPathExtension:(NSString *)pathExtension;
@property (nullable, readonly, copy) NSURL *URLByDeletingPathExtension;

@property (nullable, readonly, copy) NSURL *URLByStandardizingPath;
@property (nullable, readonly, copy) NSURL *URLByResolvingSymlinksInPath;

@end

NS_ASSUME_NONNULL_END
