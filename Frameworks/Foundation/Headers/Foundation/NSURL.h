/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */
#import <Foundation/NSObject.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSDictionary.h>

@class NSURLHandle, NSNumber, NSData, NSError;

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

- initWithScheme:(NSString *)scheme host:(NSString *)host path:(NSString *)path;
- initFileURLWithPath:(NSString *)path;
- initWithString:(NSString *)string;
- initWithString:(NSString *)string relativeToURL:(NSURL *)parent;

+ fileURLWithPath:(NSString *)path;
+ URLWithString:(NSString *)string;
+ URLWithString:(NSString *)string relativeToURL:(NSURL *)parent;

- (NSString *)absoluteString;
- (NSString *)parameterString;
- propertyForKey:(NSString *)key;

- (NSString *)scheme;
- (NSString *)host;
- (NSString *)user;
- (NSString *)password;
- (NSString *)fragment;
- (NSString *)path;
- (NSNumber *)port;
- (NSString *)query;
- (NSString *)relativePath;
- (NSString *)relativeString;
- (NSString *)resourceSpecifier;

- (BOOL)isFileURL;

- (NSURL *)standardizedURL;
- (NSURL *)absoluteURL;
- (NSURL *)baseURL;

- (NSURL *)URLByAppendingPathComponent:(NSString *)pathComponent;
- (NSURL *)URLByAppendingPathExtension:(NSString *)pathExtension;
- (NSURL *)URLByDeletingLastPathComponent;
- (NSURL *)URLByDeletingPathExtension;
- (NSString *)lastPathComponent;
- (NSString *)pathExtension;

- (BOOL)setProperty:property forKey:(NSString *)key;

- (BOOL)setResourceData:(NSData *)data;

- (NSData *)resourceDataUsingCache:(BOOL)useCache;
- (NSURLHandle *)URLHandleUsingCache:(BOOL)useCache;
- (void)loadResourceDataNotifyingClient:client usingCache:(BOOL)useCache;

@end

typedef NSString *NSURLResourceKey;
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

typedef NSString *NSURLFileResourceType;
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
+ (NSURL *)fileURLWithPath:(NSString *)path relativeToURL:(NSURL *)baseURL;
+ (NSURL *)fileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL;
+ (NSURL *)fileURLWithFileSystemRepresentation:(const char *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL;
- (instancetype)initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir;
- (instancetype)initFileURLWithPath:(NSString *)path relativeToURL:(NSURL *)baseURL;
- (instancetype)initFileURLWithPath:(NSString *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL;
- (instancetype)initFileURLWithFileSystemRepresentation:(const char *)path isDirectory:(BOOL)isDir relativeToURL:(NSURL *)baseURL;

- (BOOL)getFileSystemRepresentation:(char *)buffer maxLength:(NSUInteger)maxBufferLength;
@property (readonly) const char *fileSystemRepresentation;
@property (readonly) BOOL hasDirectoryPath;
- (BOOL)isFileReferenceURL;
- (NSURL *)fileReferenceURL;
@property (readonly, copy) NSURL *filePathURL;

- (BOOL)getResourceValue:(out id *)value forKey:(NSURLResourceKey)key error:(out NSError **)error;
- (NSDictionary<NSURLResourceKey, id> *)resourceValuesForKeys:(NSArray<NSURLResourceKey> *)keys error:(NSError **)error;
- (BOOL)setResourceValue:(id)value forKey:(NSURLResourceKey)key error:(NSError **)error;
- (BOOL)setResourceValues:(NSDictionary<NSURLResourceKey, id> *)keyedValues error:(NSError **)error;
- (void)removeCachedResourceValueForKey:(NSURLResourceKey)key;
- (void)removeAllCachedResourceValues;
- (void)setTemporaryResourceValue:(id)value forKey:(NSURLResourceKey)key;
- (BOOL)checkResourceIsReachableAndReturnError:(NSError **)error;

- (NSData *)bookmarkDataWithOptions:(NSURLBookmarkCreationOptions)options
     includingResourceValuesForKeys:(NSArray<NSURLResourceKey> *)keys
                      relativeToURL:(NSURL *)relativeURL error:(NSError **)error;
- (instancetype)initByResolvingBookmarkData:(NSData *)bookmarkData options:(NSURLBookmarkResolutionOptions)options
                              relativeToURL:(NSURL *)relativeURL bookmarkDataIsStale:(BOOL *)isStale
                                      error:(NSError **)error;
+ (instancetype)URLByResolvingBookmarkData:(NSData *)bookmarkData options:(NSURLBookmarkResolutionOptions)options
                             relativeToURL:(NSURL *)relativeURL bookmarkDataIsStale:(BOOL *)isStale
                                     error:(NSError **)error;
+ (NSDictionary<NSURLResourceKey, id> *)resourceValuesForKeys:(NSArray<NSURLResourceKey> *)keys
                                             fromBookmarkData:(NSData *)bookmarkData;
+ (BOOL)writeBookmarkData:(NSData *)bookmarkData toURL:(NSURL *)bookmarkFileURL
                  options:(NSURLBookmarkFileCreationOptions)options error:(NSError **)error;
+ (NSData *)bookmarkDataWithContentsOfURL:(NSURL *)bookmarkFileURL error:(NSError **)error;
+ (instancetype)URLByResolvingAliasFileAtURL:(NSURL *)url options:(NSURLBookmarkResolutionOptions)options
                                       error:(NSError **)error;
- (BOOL)startAccessingSecurityScopedResource;
- (void)stopAccessingSecurityScopedResource;

@end

@interface NSURL (NSURLPathUtilities)

+ (NSURL *)fileURLWithPathComponents:(NSArray *)components;
- (NSArray *)pathComponents;
- (NSURL *)URLByAppendingPathComponent:(NSString *)pathComponent isDirectory:(BOOL)isDirectory;
- (NSURL *)URLByAppendingPathExtension:(NSString *)pathExtension;
- (NSURL *)URLByDeletingPathExtension;

- (NSURL *)URLByStandardizingPath;
- (NSURL *)URLByResolvingSymlinksInPath;

@end
