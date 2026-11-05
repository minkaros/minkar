/*
 * SPDX-License-Identifier: BSD-2-Clause
 *
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#import <Foundation/NSObject.h>

@class NSDictionary<KeyType, ObjectType>, NSString, NSNumber, NSURL;

NS_ASSUME_NONNULL_BEGIN

typedef NSString *NSProgressKind;
typedef NSString *NSProgressUserInfoKey;

typedef NSString *NSProgressFileOperationKind;

FOUNDATION_EXPORT NSProgressKind const NSProgressKindFile;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressEstimatedTimeRemainingKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressThroughputKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileOperationKindKey;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindDownloading;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindDecompressingAfterDownloading;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindReceiving;
FOUNDATION_EXPORT NSProgressFileOperationKind const NSProgressFileOperationKindCopying;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileURLKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileTotalCountKey;
FOUNDATION_EXPORT NSProgressUserInfoKey const NSProgressFileCompletedCountKey;

// Progress of a task: units done out of a total, in a tree whose parents
// count their children's progress as pending units. Changes to
// completedUnitCount, totalUnitCount and fractionCompleted are observable.
@interface NSProgress : NSObject

+ (nullable NSProgress *)currentProgress;
+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount;
+ (NSProgress *)discreteProgressWithTotalUnitCount:(int64_t)unitCount;
+ (NSProgress *)progressWithTotalUnitCount:(int64_t)unitCount parent:(NSProgress *)parent
                          pendingUnitCount:(int64_t)portionOfParentTotalUnitCount;

- (instancetype)initWithParent:(nullable NSProgress *)parentOrNil
                      userInfo:(nullable NSDictionary *)userInfoOrNil;

- (void)becomeCurrentWithPendingUnitCount:(int64_t)unitCount;
- (void)resignCurrent;
- (void)addChild:(NSProgress *)child withPendingUnitCount:(int64_t)inUnitCount;

@property int64_t totalUnitCount;
@property int64_t completedUnitCount;
@property (readonly) double fractionCompleted;
@property (readonly, getter=isIndeterminate) BOOL indeterminate;
@property (readonly, getter=isFinished) BOOL finished;

@property (null_resettable, copy) NSString *localizedDescription;
@property (null_resettable, copy) NSString *localizedAdditionalDescription;

@property (getter=isCancellable) BOOL cancellable;
@property (getter=isPausable) BOOL pausable;
@property (readonly, getter=isCancelled) BOOL cancelled;
@property (readonly, getter=isPaused) BOOL paused;
@property (nullable, copy) void (^cancellationHandler)(void);
@property (nullable, copy) void (^pausingHandler)(void);
@property (nullable, copy) void (^resumingHandler)(void);

- (void)cancel;
- (void)pause;
- (void)resume;

@property (nullable, copy) NSProgressKind kind;
@property (readonly, copy) NSDictionary *userInfo;
- (void)setUserInfoObject:(nullable id)objectOrNil forKey:(NSProgressUserInfoKey)key;

/* The user info's values, as properties. */
@property (nullable, copy) NSNumber *estimatedTimeRemaining NS_REFINED_FOR_SWIFT;
@property (nullable, copy) NSNumber *throughput NS_REFINED_FOR_SWIFT;
@property (nullable, copy) NSProgressFileOperationKind fileOperationKind;
@property (nullable, copy) NSURL *fileURL;
@property (nullable, copy) NSNumber *fileTotalCount NS_REFINED_FOR_SWIFT;
@property (nullable, copy) NSNumber *fileCompletedCount NS_REFINED_FOR_SWIFT;

- (void)performAsCurrentWithPendingUnitCount:(int64_t)unitCount usingBlock:(void (NS_NOESCAPE ^)(void))work;

@end

@protocol NSProgressReporting <NSObject>
@property (readonly) NSProgress *progress;
@end


NS_ASSUME_NONNULL_END
