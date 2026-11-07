/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSDirectoryEnumerator.h>
#import <Foundation/NSPathUtilities.h>
#import <Foundation/NSURL.h>

@class NSData, NSDate, NSError, NSNumber, NSURL;

NS_ASSUME_NONNULL_BEGIN

typedef NSString *NSFileAttributeKey NS_TYPED_EXTENSIBLE_ENUM;
typedef NSString *NSFileAttributeType NS_TYPED_ENUM;

FOUNDATION_EXPORT NSFileAttributeKey const NSFileType;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeDirectory;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeRegular;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeSymbolicLink;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeSocket;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeCharacterSpecial;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeBlockSpecial;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeFIFO;
FOUNDATION_EXPORT NSFileAttributeType const NSFileTypeUnknown;

FOUNDATION_EXPORT NSFileAttributeKey const NSFileSize;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileModificationDate;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileReferenceCount;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileDeviceIdentifier;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileOwnerAccountName;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileGroupOwnerAccountName;
FOUNDATION_EXPORT NSFileAttributeKey const NSFilePosixPermissions;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileSystemNumber;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileSystemFileNumber;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileExtensionHidden;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileHFSCreatorCode;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileHFSTypeCode;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileImmutable;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileAppendOnly;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileCreationDate;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileOwnerAccountID;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileGroupOwnerAccountID;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileBusy;
// Minkar's own.
FOUNDATION_EXPORT NSFileAttributeKey const NSFileIdentifier;

FOUNDATION_EXPORT NSFileAttributeKey const NSFileSystemSize;
FOUNDATION_EXPORT NSFileAttributeKey const NSFileSystemFreeSize;

typedef NS_OPTIONS(NSUInteger, NSDirectoryEnumerationOptions) {
    NSDirectoryEnumerationSkipsSubdirectoryDescendants = 1UL << 0,
    NSDirectoryEnumerationSkipsPackageDescendants = 1UL << 1,
    NSDirectoryEnumerationSkipsHiddenFiles = 1UL << 2,
    NSDirectoryEnumerationIncludesDirectoriesPostOrder = 1UL << 3,
    NSDirectoryEnumerationProducesRelativePathURLs = 1UL << 4,
};

typedef NS_OPTIONS(NSUInteger, NSFileManagerItemReplacementOptions) {
    NSFileManagerItemReplacementUsingNewMetadataOnly = 1UL << 0,
    NSFileManagerItemReplacementWithoutDeletingBackupItem = 1UL << 1
};

typedef NS_ENUM(NSInteger, NSURLRelationship) {
    NSURLRelationshipContains,
    NSURLRelationshipSame,
    NSURLRelationshipOther
};

@interface NSFileManager : NSObject

@property (class, readonly, strong) NSFileManager *defaultManager;

@property (nullable, assign) id delegate;

- (nullable NSDictionary<NSFileAttributeKey, id> *)attributesOfFileSystemForPath:(NSString *)path error:(NSError **)errorp;
- (nullable NSDictionary<NSFileAttributeKey, id> *)attributesOfItemAtPath:(NSString *)path error:(NSError **)error;
- (BOOL)changeCurrentDirectoryPath:(NSString *)path;
- (nullable NSArray<NSString *> *)componentsToDisplayForPath:(NSString *)path;
- (BOOL)contentsEqualAtPath:(NSString *)path1 andPath:(NSString *)path2;
- (nullable NSArray<NSString *> *)contentsOfDirectoryAtPath:(NSString *)path error:(NSError **)error;
- (BOOL)copyItemAtPath:(NSString *)fromPath toPath:(NSString *)toPath error:(NSError **)error;
- (nullable NSString *)destinationOfSymbolicLinkAtPath:(NSString *)path error:(NSError **)error;

- (NSString *)displayNameAtPath:(NSString *)path;

- (nullable NSDictionary *)fileSystemAttributesAtPath:(NSString *)path;

- (BOOL)isDeletableFileAtPath:(NSString *)path;

- (BOOL)linkItemAtPath:(NSString *)fromPath toPath:(NSString *)toPath error:(NSError **)error;
- (BOOL)linkPath:(NSString *)source toPath:(NSString *)destination handler:(nullable id)handler;
- (BOOL)moveItemAtPath:(NSString *)fromPath toPath:(NSString *)toPath error:(NSError **)error;
- (BOOL)removeItemAtPath:(NSString *)path error:(NSError **)error;

- (BOOL)copyItemAtURL:(NSURL *)srcURL toURL:(NSURL *)dstURL error:(NSError **)error;
- (BOOL)moveItemAtURL:(NSURL *)srcURL toURL:(NSURL *)dstURL error:(NSError **)error;
- (BOOL)linkItemAtURL:(NSURL *)srcURL toURL:(NSURL *)dstURL error:(NSError **)error;
- (BOOL)removeItemAtURL:(NSURL *)URL error:(NSError **)error;

- (BOOL)setAttributes:(NSDictionary<NSFileAttributeKey, id> *)attributes ofItemAtPath:(NSString *)path error:(NSError **)error;

- (NSString *)stringWithFileSystemRepresentation:(const char *)string length:(NSUInteger)length;

- (nullable NSArray<NSString *> *)subpathsAtPath:(NSString *)path;
- (nullable NSArray<NSString *> *)subpathsOfDirectoryAtPath:(NSString *)path error:(NSError **)error;

- (nullable NSData *)contentsAtPath:(NSString *)path;

- (BOOL)createFileAtPath:(NSString *)path contents:(nullable NSData *)data attributes:(nullable NSDictionary<NSFileAttributeKey, id> *)attributes;

- (nullable NSArray *)directoryContentsAtPath:(NSString *)path;
- (nullable NSDirectoryEnumerator<NSString *> *)enumeratorAtPath:(NSString *)path;

- (BOOL)createDirectoryAtPath:(NSString *)path attributes:(nullable NSDictionary *)attributes;
- (BOOL)createDirectoryAtPath:(NSString *)path withIntermediateDirectories:(BOOL)intermediates attributes:(nullable NSDictionary<NSFileAttributeKey, id> *)attributes error:(NSError **)error;

- (BOOL)createDirectoryAtURL:(NSURL *)url withIntermediateDirectories:(BOOL)intermediates attributes:(nullable NSDictionary<NSFileAttributeKey, id> *)attributes error:(NSError **)error;

- (BOOL)createSymbolicLinkAtPath:(NSString *)path pathContent:(NSString *)destination;
- (BOOL)createSymbolicLinkAtPath:(NSString *)path withDestinationPath:(NSString *)toPath error:(NSError **)error;

- (nullable NSString *)pathContentOfSymbolicLinkAtPath:(NSString *)path;

- (BOOL)fileExistsAtPath:(NSString *)path;
- (BOOL)fileExistsAtPath:(NSString *)path isDirectory:(nullable BOOL *)isDirectory;

- (BOOL)removeFileAtPath:(NSString *)path handler:(nullable id)handler;

- (BOOL)movePath:(NSString *)src toPath:(NSString *)dest handler:(nullable id)handler;
- (BOOL)copyPath:(NSString *)src toPath:(NSString *)dest handler:(nullable id)handler;

@property (readonly, copy) NSString *currentDirectoryPath;

- (nullable NSDictionary *)fileAttributesAtPath:(NSString *)path traverseLink:(BOOL)traverse;

- (BOOL)isReadableFileAtPath:(NSString *)path;
- (BOOL)isWritableFileAtPath:(NSString *)path;
- (BOOL)isExecutableFileAtPath:(NSString *)path;

- (BOOL)changeFileAttributes:(NSDictionary *)attributes atPath:(NSString *)path;

- (const char *)fileSystemRepresentationWithPath:(NSString *)path NS_RETURNS_INNER_POINTER;
- (const uint16_t *)fileSystemRepresentationWithPathW:(NSString *)path NS_RETURNS_INNER_POINTER;

@end

@interface NSFileManager (NSURLResources)
- (NSArray<NSURL *> *)URLsForDirectory:(NSSearchPathDirectory)directory inDomains:(NSSearchPathDomainMask)domainMask;
- (nullable NSURL *)URLForDirectory:(NSSearchPathDirectory)directory inDomain:(NSSearchPathDomainMask)domain
                  appropriateForURL:(nullable NSURL *)url create:(BOOL)shouldCreate error:(NSError **)error;
@property (readonly, copy) NSURL *homeDirectoryForCurrentUser;
@property (readonly, copy) NSURL *temporaryDirectory;
- (nullable NSURL *)homeDirectoryForUser:(NSString *)userName;
- (nullable NSArray<NSURL *> *)contentsOfDirectoryAtURL:(NSURL *)url includingPropertiesForKeys:(nullable NSArray<NSURLResourceKey> *)keys
                                                options:(NSDirectoryEnumerationOptions)mask error:(NSError **)error;
- (nullable NSDirectoryEnumerator<NSURL *> *)enumeratorAtURL:(NSURL *)url includingPropertiesForKeys:(nullable NSArray<NSURLResourceKey> *)keys
                                                     options:(NSDirectoryEnumerationOptions)mask
                                                errorHandler:(nullable BOOL (^)(NSURL *url, NSError *error))handler;
- (BOOL)replaceItemAtURL:(NSURL *)originalItemURL withItemAtURL:(NSURL *)newItemURL
          backupItemName:(nullable NSString *)backupItemName options:(NSFileManagerItemReplacementOptions)options
        resultingItemURL:(NSURL *_Nullable *_Nullable)resultingURL error:(NSError **)error;
- (BOOL)trashItemAtURL:(NSURL *)url resultingItemURL:(NSURL *_Nullable *_Nullable)outResultingURL error:(NSError **)error;
- (BOOL)createSymbolicLinkAtURL:(NSURL *)url withDestinationURL:(NSURL *)destURL error:(NSError **)error;
- (BOOL)getRelationship:(NSURLRelationship *)outRelationship ofDirectoryAtURL:(NSURL *)directoryURL
           toItemAtURL:(NSURL *)otherURL error:(NSError **)error;
- (BOOL)getRelationship:(NSURLRelationship *)outRelationship ofDirectory:(NSSearchPathDirectory)directory
              inDomain:(NSSearchPathDomainMask)domainMask toItemAtURL:(NSURL *)url error:(NSError **)error;
- (nullable NSURL *)URLForUbiquityContainerIdentifier:(nullable NSString *)containerIdentifier;
@property (nullable, readonly, copy) id ubiquityIdentityToken;
@end


@interface NSObject (NSFileManager_handler)
- (BOOL)fileManager:(NSFileManager *)fileManager shouldProceedAfterError:(NSDictionary *)dictionary;
- (void)fileManager:(NSFileManager *)fileManager willProcessPath:(NSString *)path;
@end

@interface NSObject (NSFileManagerDelegate)
- (BOOL)fileManager:(NSFileManager *)fileManager shouldCopyItemAtPath:(NSString *)path toPath:(NSString *)toPath;
- (BOOL)fileManager:(NSFileManager *)fileManager shouldLinkItemAtPath:(NSString *)path toPath:(NSString *)toPath;
- (BOOL)fileManager:(NSFileManager *)fileManager shouldMoveItemAtPath:(NSString *)path toPath:(NSString *)toPath;
- (BOOL)fileManager:(NSFileManager *)fileManager shouldProceedAfterError:(NSError *)error copyingItemAtPath:(NSString *)path toPath:(NSString *)toPath;
- (BOOL)fileManager:(NSFileManager *)fileManager shouldProceedAfterError:(NSError *)error linkingItemAtPath:(NSString *)path toPath:(NSString *)toPath;
- (BOOL)fileManager:(NSFileManager *)fileManager shouldProceedAfterError:(NSError *)error movingItemAtPath:(NSString *)path toPath:(NSString *)toPath;
- (BOOL)fileManager:(NSFileManager *)fileManager shouldProceedAfterError:(NSError *)error removingItemAtPath:(NSString *)path;

- (BOOL)fileManager:(NSFileManager *)fileManager shouldRemoveItemAtPath:(NSString *)path;

@end

@interface NSDictionary<KeyType, ObjectType> (NSFileManager_fileAttributes)
- (unsigned long long)fileSize;
- (nullable NSDate *)fileModificationDate;
- (nullable NSString *)fileType;
- (NSUInteger)filePosixPermissions;
- (nullable NSString *)fileOwnerAccountName;
- (nullable NSString *)fileGroupOwnerAccountName;
- (NSInteger)fileSystemNumber;
- (NSUInteger)fileSystemFileNumber;
- (BOOL)fileExtensionHidden;
- (OSType)fileHFSCreatorCode;
- (OSType)fileHFSTypeCode;
- (BOOL)fileIsImmutable;
- (BOOL)fileIsAppendOnly;
- (nullable NSDate *)fileCreationDate;
- (nullable NSNumber *)fileOwnerAccountID;
- (nullable NSNumber *)fileGroupOwnerAccountID;
@end

NS_ASSUME_NONNULL_END
