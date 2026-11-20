// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <AppKit/NSPasteboard.h>

@class NSFilePromiseProvider, NSURL, NSString, NSOperationQueue;

NS_ASSUME_NONNULL_BEGIN

@protocol NSFilePromiseProviderDelegate <NSObject>
- (NSString *)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider fileNameForType:(NSString *)fileType;
- (void)filePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider writePromiseToURL:(NSURL *)url completionHandler:(void (^)(NSError *_Nullable errorOrNil))completionHandler;
@optional
- (NSOperationQueue *)operationQueueForFilePromiseProvider:(NSFilePromiseProvider *)filePromiseProvider;
@end

/* A file promised on a pasteboard (a drag): written by its delegate where
   the destination asks, once it asks. */
@interface NSFilePromiseProvider : NSObject <NSPasteboardWriting>
- (instancetype)initWithFileType:(NSString *)fileType delegate:(id<NSFilePromiseProviderDelegate>)delegate;
- (instancetype)init;
@property (copy) NSString *fileType;
@property (assign, nullable) id<NSFilePromiseProviderDelegate> delegate;
@property (strong, nullable) id userInfo;
@end

NS_ASSUME_NONNULL_END
