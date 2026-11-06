/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */
#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSMetadataItem.h>
#import <Foundation/NSMetadataQueryAttributeValueTuple.h>
#import <Foundation/NSMetadataQueryResultGroup.h>
#import <Foundation/NSMetadataAttributes.h>

@class NSPredicate, NSArray<ObjectType>, NSDictionary<KeyType, ObjectType>, NSOperationQueue, NSSortDescriptor, NSMutableArray, NSThread;
@protocol NSMetadataQueryDelegate;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSMetadataQueryDidStartGatheringNotification;
FOUNDATION_EXPORT NSString *const NSMetadataQueryGatheringProgressNotification;
FOUNDATION_EXPORT NSString *const NSMetadataQueryDidFinishGatheringNotification;
FOUNDATION_EXPORT NSString *const NSMetadataQueryDidUpdateNotification;

FOUNDATION_EXPORT NSString *const NSMetadataQueryUpdateAddedItemsKey;
FOUNDATION_EXPORT NSString *const NSMetadataQueryUpdateChangedItemsKey;
FOUNDATION_EXPORT NSString *const NSMetadataQueryUpdateRemovedItemsKey;
FOUNDATION_EXPORT NSString *const NSMetadataQueryResultContentRelevanceAttribute;

FOUNDATION_EXPORT NSString *const NSMetadataQueryUserHomeScope;
FOUNDATION_EXPORT NSString *const NSMetadataQueryLocalComputerScope;
FOUNDATION_EXPORT NSString *const NSMetadataQueryNetworkScope;
FOUNDATION_EXPORT NSString *const NSMetadataQueryIndexedLocalComputerScope;
FOUNDATION_EXPORT NSString *const NSMetadataQueryIndexedNetworkScope;

/* A search of the files in some folders for those that match a predicate
 * on their attributes. There is no index: the scopes are walked when the
 * query starts. */
@interface NSMetadataQuery : NSObject {
    id _delegate;
    NSPredicate *_predicate;
    NSArray *_sortDescriptors;
    NSArray *_valueListAttributes;
    NSArray *_groupingAttributes;
    NSArray *_searchScopes;
    NSArray *_searchItems;
    NSOperationQueue *_operationQueue;
    NSTimeInterval _notificationBatchingInterval;
    NSArray *_results;
    NSDictionary *_valueLists;
    NSThread *_thread;
    BOOL _started, _gathering, _stopped;
    NSUInteger _disabled;
}

@property (nullable, assign) id<NSMetadataQueryDelegate> delegate;
@property (nullable, copy) NSPredicate *predicate;
@property (copy) NSArray<NSSortDescriptor *> *sortDescriptors;
@property (copy) NSArray<NSString *> *valueListAttributes;
@property (nullable, copy) NSArray<NSString *> *groupingAttributes;
@property NSTimeInterval notificationBatchingInterval;
@property (copy) NSArray *searchScopes;
@property (nullable, copy) NSArray *searchItems;
@property (nullable, retain) NSOperationQueue *operationQueue;

- (BOOL)startQuery;
- (void)stopQuery;

@property (readonly, getter=isStarted) BOOL started;
@property (readonly, getter=isGathering) BOOL gathering;
@property (readonly, getter=isStopped) BOOL stopped;

- (void)disableUpdates;
- (void)enableUpdates;

@property (readonly) NSUInteger resultCount;
- (id)resultAtIndex:(NSUInteger)idx;
- (void)enumerateResultsUsingBlock:(void (NS_NOESCAPE ^)(id result, NSUInteger idx, BOOL *stop))block;
- (void)enumerateResultsWithOptions:(NSEnumerationOptions)opts usingBlock:(void (NS_NOESCAPE ^)(id result, NSUInteger idx, BOOL *stop))block;
@property (readonly, copy) NSArray *results;
- (NSUInteger)indexOfResult:(id)result;

@property (readonly, copy) NSDictionary<NSString *, NSArray<NSMetadataQueryAttributeValueTuple *> *> *valueLists;
@property (readonly, copy) NSArray<NSMetadataQueryResultGroup *> *groupedResults;

- (nullable id)valueOfAttribute:(NSString *)attrName forResultAtIndex:(NSUInteger)idx;

@end

@protocol NSMetadataQueryDelegate <NSObject>
@optional
- (id)metadataQuery:(NSMetadataQuery *)query replacementObjectForResultObject:(NSMetadataItem *)result;
- (id)metadataQuery:(NSMetadataQuery *)query replacementValueForAttribute:(NSString *)attrName value:(id)attrValue;
@end

NS_ASSUME_NONNULL_END
