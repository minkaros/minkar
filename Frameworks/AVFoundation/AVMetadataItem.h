/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * A piece of an asset's or a track's metadata: its key in a key space
 * (or its identifier, which is both), its value, and when it applies.
 */

#import <Foundation/Foundation.h>
#import <CoreMedia/CMTime.h>
#import <AVFoundation/AVAsynchronousKeyValueLoading.h>
#import <AVFoundation/AVMetadataFormat.h>

NS_ASSUME_NONNULL_BEGIN

@interface AVMetadataItem : NSObject <AVAsynchronousKeyValueLoading, NSCopying, NSMutableCopying>
@property (readonly, copy, nullable) AVMetadataIdentifier identifier;
@property (readonly, copy, nullable) NSString *extendedLanguageTag;
@property (readonly, copy, nullable) NSLocale *locale;
@property (readonly) CMTime time;
@property (readonly) CMTime duration;
@property (readonly, copy, nullable) NSString *dataType;
@property (readonly, copy, nullable) id<NSObject, NSCopying> value;
@property (readonly, copy, nullable) NSDictionary<AVMetadataExtraAttributeKey, id> *extraAttributes;
@property (readonly, copy, nullable) id<NSObject, NSCopying> key;
@property (readonly, copy, nullable) AVMetadataKey commonKey;
@property (readonly, copy, nullable) AVMetadataKeySpace keySpace;
@property (readonly, nullable) NSString *stringValue;
@property (readonly, nullable) NSNumber *numberValue;
@property (readonly, nullable) NSDate *dateValue;
@property (readonly, nullable) NSData *dataValue;
@property (readonly, nullable) NSDate *startDate;

+ (nullable AVMetadataIdentifier)identifierForKey:(id)key keySpace:(AVMetadataKeySpace)keySpace;
+ (nullable AVMetadataKeySpace)keySpaceForIdentifier:(AVMetadataIdentifier)identifier;
+ (nullable id)keyForIdentifier:(AVMetadataIdentifier)identifier;
+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)metadataItems
                                              withKey:(nullable id)key
                                             keySpace:(nullable AVMetadataKeySpace)keySpace;
+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)metadataItems
                                 filteredByIdentifier:(AVMetadataIdentifier)identifier;
+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)metadataItems
                                           withLocale:(NSLocale *)locale;
+ (NSArray<AVMetadataItem *> *)metadataItemsFromArray:(NSArray<AVMetadataItem *> *)metadataItems
                             filteredAndSortedAccordingToPreferredLanguages:(NSArray<NSString *> *)preferredLanguages;
@end

@interface AVMutableMetadataItem : AVMetadataItem
+ (AVMutableMetadataItem *)metadataItem;
@property (readwrite, copy, nullable) AVMetadataIdentifier identifier;
@property (readwrite, copy, nullable) NSString *extendedLanguageTag;
@property (readwrite, copy, nullable) NSLocale *locale;
@property (readwrite) CMTime time;
@property (readwrite) CMTime duration;
@property (readwrite, copy, nullable) NSString *dataType;
@property (readwrite, copy, nullable) id<NSObject, NSCopying> value;
@property (readwrite, copy, nullable) NSDictionary<AVMetadataExtraAttributeKey, id> *extraAttributes;
@property (readwrite, copy, nullable) id<NSObject, NSCopying> key;
@property (readwrite, copy, nullable) AVMetadataKeySpace keySpace;
@property (readwrite, copy, nullable) NSDate *startDate;
@end

NS_ASSUME_NONNULL_END
