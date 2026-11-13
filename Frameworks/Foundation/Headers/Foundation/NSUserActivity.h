/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSUserActivity (API 19): what the user is doing in an app (a web page,
 * a document), kept so the app can offer to continue it. Minkar has no
 * service that carries activities to other devices; an activity is kept
 * in its process, and the current one is the one last made current.
 */

#import <Foundation/NSObject.h>

#import <Foundation/NSArray.h>
#import <Foundation/NSSet.h>

@class NSDate, NSDictionary, NSError, NSInputStream, NSOutputStream, NSString, NSURL;
@protocol NSUserActivityDelegate;

NS_ASSUME_NONNULL_BEGIN

typedef NSString *NSUserActivityPersistentIdentifier;

/* The type of an activity that is a web page, continued in a browser. */
FOUNDATION_EXPORT NSString *const NSUserActivityTypeBrowsingWeb;

@interface NSUserActivity : NSObject

- (instancetype)initWithActivityType:(NSString *)activityType NS_DESIGNATED_INITIALIZER;
- (instancetype)init;

@property (readonly, copy) NSString *activityType;
@property (nullable, copy) NSString *title;
@property (nullable, copy) NSDictionary *userInfo;
- (void)addUserInfoEntriesFromDictionary:(NSDictionary *)otherDictionary;
@property (nullable, copy) NSSet<NSString *> *requiredUserInfoKeys;
@property BOOL needsSave;
@property (nullable, copy) NSURL *webpageURL;
@property (nullable, copy) NSURL *referrerURL;
@property (nullable, copy) NSDate *expirationDate;
@property (copy) NSSet<NSString *> *keywords;
@property BOOL supportsContinuationStreams;
@property (nullable, weak) id<NSUserActivityDelegate> delegate;
@property (nullable, copy) NSString *targetContentIdentifier;
@property (getter=isEligibleForHandoff) BOOL eligibleForHandoff;
@property (getter=isEligibleForSearch) BOOL eligibleForSearch;
@property (getter=isEligibleForPublicIndexing) BOOL eligibleForPublicIndexing;
@property (getter=isEligibleForPrediction) BOOL eligibleForPrediction;
@property (nullable, copy) NSUserActivityPersistentIdentifier persistentIdentifier;

- (void)becomeCurrent;
- (void)resignCurrent;
- (void)invalidate;

- (void)getContinuationStreamsWithCompletionHandler:(void (^)(NSInputStream * _Nullable inputStream,
    NSOutputStream * _Nullable outputStream, NSError * _Nullable error))completionHandler;

+ (void)deleteSavedUserActivitiesWithPersistentIdentifiers:(NSArray<NSUserActivityPersistentIdentifier> *)persistentIdentifiers
    completionHandler:(void (^)(void))handler;
+ (void)deleteAllSavedUserActivitiesWithCompletionHandler:(void (^)(void))handler;

@end

@protocol NSUserActivityDelegate <NSObject>
@optional
- (void)userActivityWillSave:(NSUserActivity *)userActivity;
- (void)userActivityWasContinued:(NSUserActivity *)userActivity;
- (void)userActivity:(NSUserActivity *)userActivity didReceiveInputStream:(NSInputStream *)inputStream
    outputStream:(NSOutputStream *)outputStream;
@end

NS_ASSUME_NONNULL_END
