/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSFileVersion: earlier contents of a file, kept on the file system so a
 * document can go back to them. Minkar keeps each user's in
 * ~/Library/Versions, one folder per document's path.
 */

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>

@class NSArray, NSError, NSString, NSURL;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSFileVersionAddingOptions) {
    NSFileVersionAddingByMoving = 1 << 0,
};

typedef NS_OPTIONS(NSUInteger, NSFileVersionReplacingOptions) {
    NSFileVersionReplacingByMoving = 1 << 0,
};

@interface NSFileVersion : NSObject {
    NSURL *_itemURL;
    NSURL *_URL;
    NSString *_identifier;
    NSString *_localizedName;
    NSDate *_modificationDate;
    BOOL _isCurrent;
    BOOL _discardable;
    BOOL _resolved;
}

+ (nullable NSFileVersion *)currentVersionOfItemAtURL:(NSURL *)url;
+ (nullable NSArray<NSFileVersion *> *)otherVersionsOfItemAtURL:(NSURL *)url;
+ (nullable NSArray<NSFileVersion *> *)unresolvedConflictVersionsOfItemAtURL:(NSURL *)url;
+ (nullable NSFileVersion *)versionOfItemAtURL:(NSURL *)url forPersistentIdentifier:(id)persistentIdentifier;
+ (nullable NSFileVersion *)addVersionOfItemAtURL:(NSURL *)url withContentsOfURL:(NSURL *)contentsURL
    options:(NSFileVersionAddingOptions)options error:(NSError **)outError;
+ (NSURL *)temporaryDirectoryURLForNewVersionOfItemAtURL:(NSURL *)url;
+ (BOOL)removeOtherVersionsOfItemAtURL:(NSURL *)url error:(NSError **)outError;

@property (readonly, copy) NSURL *URL;
@property (nullable, readonly, copy) NSString *localizedName;
@property (nullable, readonly, copy) NSString *localizedNameOfSavingComputer;
@property (nullable, readonly, copy) NSDate *modificationDate;
@property (readonly, retain) id<NSCoding> persistentIdentifier;
@property (readonly, getter=isConflict) BOOL conflict;
@property (getter=isResolved) BOOL resolved;
@property (getter=isDiscardable) BOOL discardable;
@property (readonly) BOOL hasLocalContents;
@property (readonly) BOOL hasThumbnail;

- (nullable NSURL *)replaceItemAtURL:(NSURL *)url options:(NSFileVersionReplacingOptions)options
    error:(NSError **)error;
- (BOOL)removeAndReturnError:(NSError **)outError;

@end

NS_ASSUME_NONNULL_END
