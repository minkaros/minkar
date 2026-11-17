/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Coordinated reading and writing of files and folders: readers wait
 * for writers, writers for everyone, and the item's presenters
 * (NSFilePresenter) save, let go and hear of changes around each access.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSFilePresenter.h>
#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>

@class NSError, NSOperationQueue, NSString, NSURL;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSFileCoordinatorReadingOptions) {
    NSFileCoordinatorReadingWithoutChanges = 1 << 0,
    NSFileCoordinatorReadingResolvesSymbolicLink = 1 << 1,
    NSFileCoordinatorReadingImmediatelyAvailableMetadataOnly = 1 << 2,
    NSFileCoordinatorReadingForUploading = 1 << 3,
};

typedef NS_OPTIONS(NSUInteger, NSFileCoordinatorWritingOptions) {
    NSFileCoordinatorWritingForDeleting = 1 << 0,
    NSFileCoordinatorWritingForMoving = 1 << 1,
    NSFileCoordinatorWritingForMerging = 1 << 2,
    NSFileCoordinatorWritingForReplacing = 1 << 3,
    NSFileCoordinatorWritingContentIndependentMetadataOnly = 1 << 4,
};

@interface NSFileAccessIntent : NSObject
+ (instancetype)readingIntentWithURL:(NSURL *)url options:(NSFileCoordinatorReadingOptions)options;
+ (instancetype)writingIntentWithURL:(NSURL *)url options:(NSFileCoordinatorWritingOptions)options;
/* The URL to use in the accessor: the coordinated item, wherever it is now. */
@property (readonly, copy) NSURL *URL;
@end

@interface NSFileCoordinator : NSObject

+ (void)addFilePresenter:(id<NSFilePresenter>)filePresenter;
+ (void)removeFilePresenter:(id<NSFilePresenter>)filePresenter;
@property (class, readonly, copy) NSArray<id<NSFilePresenter>> *filePresenters;

- (instancetype)initWithFilePresenter:(nullable id<NSFilePresenter>)filePresenterOrNil NS_DESIGNATED_INITIALIZER;

@property (copy) NSString *purposeIdentifier;

- (void)coordinateAccessWithIntents:(NSArray<NSFileAccessIntent *> *)intents queue:(NSOperationQueue *)queue
    byAccessor:(void (^)(NSError * _Nullable error))accessor;

- (void)coordinateReadingItemAtURL:(NSURL *)url options:(NSFileCoordinatorReadingOptions)options
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *newURL))reader;
- (void)coordinateWritingItemAtURL:(NSURL *)url options:(NSFileCoordinatorWritingOptions)options
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *newURL))writer;
- (void)coordinateReadingItemAtURL:(NSURL *)readingURL options:(NSFileCoordinatorReadingOptions)readingOptions
    writingItemAtURL:(NSURL *)writingURL options:(NSFileCoordinatorWritingOptions)writingOptions
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *newReadingURL, NSURL *newWritingURL))readerWriter;
- (void)coordinateWritingItemAtURL:(NSURL *)url1 options:(NSFileCoordinatorWritingOptions)options1
    writingItemAtURL:(NSURL *)url2 options:(NSFileCoordinatorWritingOptions)options2
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(NSURL *newURL1, NSURL *newURL2))writer;
- (void)prepareForReadingItemsAtURLs:(NSArray<NSURL *> *)readingURLs options:(NSFileCoordinatorReadingOptions)readingOptions
    writingItemsAtURLs:(NSArray<NSURL *> *)writingURLs options:(NSFileCoordinatorWritingOptions)writingOptions
    error:(NSError **)outError byAccessor:(void (NS_NOESCAPE ^)(void (^completionHandler)(void)))batchAccessor;

- (void)itemAtURL:(NSURL *)oldURL willMoveToURL:(NSURL *)newURL;
- (void)itemAtURL:(NSURL *)oldURL didMoveToURL:(NSURL *)newURL;
- (void)itemAtURL:(NSURL *)url didChangeUbiquityAttributes:(NSSet<NSString *> *)attributes;

- (void)cancel;

@end

NS_ASSUME_NONNULL_END
