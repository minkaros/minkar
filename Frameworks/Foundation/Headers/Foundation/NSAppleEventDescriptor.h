// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSDate.h>
#import <CoreServices/CoreServices.h>

@class NSData, NSDate, NSString, NSURL, NSError, NSDictionary;

NS_ASSUME_NONNULL_BEGIN

typedef NS_OPTIONS(NSUInteger, NSAppleEventSendOptions) {
    NSAppleEventSendNoReply = kAENoReply,
    NSAppleEventSendQueueReply = kAEQueueReply,
    NSAppleEventSendWaitForReply = kAEWaitReply,
    NSAppleEventSendNeverInteract = kAENeverInteract,
    NSAppleEventSendCanInteract = kAECanInteract,
    NSAppleEventSendAlwaysInteract = kAEAlwaysInteract,
    NSAppleEventSendCanSwitchLayer = kAECanSwitchLayer,
    NSAppleEventSendDontRecord = kAEDontRecord,
    NSAppleEventSendDontExecute = kAEDontExecute,
    NSAppleEventSendDontAnnotate = kAEDoNotAutomaticallyAddAnnotationsToEvent,
    NSAppleEventSendDefaultOptions = NSAppleEventSendWaitForReply | NSAppleEventSendCanInteract
};

@interface NSAppleEventDescriptor : NSObject <NSCopying, NSSecureCoding> {
    AEDesc _desc;
}

+ (NSAppleEventDescriptor *)nullDescriptor;
+ (nullable NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)descriptorType bytes:(nullable const void *)bytes length:(NSUInteger)byteCount;
+ (nullable NSAppleEventDescriptor *)descriptorWithDescriptorType:(DescType)descriptorType data:(nullable NSData *)data;
+ (NSAppleEventDescriptor *)descriptorWithBoolean:(Boolean)boolean;
+ (NSAppleEventDescriptor *)descriptorWithEnumCode:(OSType)enumerator;
+ (NSAppleEventDescriptor *)descriptorWithInt32:(SInt32)signedInt;
+ (NSAppleEventDescriptor *)descriptorWithDouble:(double)doubleValue;
+ (NSAppleEventDescriptor *)descriptorWithTypeCode:(OSType)typeCode;
+ (NSAppleEventDescriptor *)descriptorWithString:(NSString *)string;
+ (NSAppleEventDescriptor *)descriptorWithDate:(NSDate *)date;
+ (NSAppleEventDescriptor *)descriptorWithFileURL:(NSURL *)fileURL;
+ (NSAppleEventDescriptor *)appleEventWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
                                    targetDescriptor:(nullable NSAppleEventDescriptor *)targetDescriptor
                                            returnID:(AEReturnID)returnID transactionID:(AETransactionID)transactionID;
+ (NSAppleEventDescriptor *)listDescriptor;
+ (NSAppleEventDescriptor *)recordDescriptor;
+ (NSAppleEventDescriptor *)currentProcessDescriptor;
+ (NSAppleEventDescriptor *)descriptorWithProcessIdentifier:(pid_t)processIdentifier;
+ (NSAppleEventDescriptor *)descriptorWithBundleIdentifier:(NSString *)bundleIdentifier;
+ (NSAppleEventDescriptor *)descriptorWithApplicationURL:(NSURL *)applicationURL;

- (instancetype)initWithAEDescNoCopy:(const AEDesc *)aeDesc NS_DESIGNATED_INITIALIZER;
- (nullable instancetype)initWithDescriptorType:(DescType)descriptorType bytes:(nullable const void *)bytes length:(NSUInteger)byteCount;
- (nullable instancetype)initWithDescriptorType:(DescType)descriptorType data:(nullable NSData *)data;
- (instancetype)initWithEventClass:(AEEventClass)eventClass eventID:(AEEventID)eventID
                  targetDescriptor:(nullable NSAppleEventDescriptor *)targetDescriptor
                          returnID:(AEReturnID)returnID transactionID:(AETransactionID)transactionID;
- (instancetype)initListDescriptor;
- (instancetype)initRecordDescriptor;

@property (nullable, readonly) const AEDesc *aeDesc NS_RETURNS_INNER_POINTER;
@property (readonly) DescType descriptorType;
@property (readonly, copy) NSData *data;
@property (readonly) Boolean booleanValue;
@property (readonly) OSType enumCodeValue;
@property (readonly) SInt32 int32Value;
@property (readonly) double doubleValue;
@property (readonly) OSType typeCodeValue;
@property (nullable, readonly, copy) NSString *stringValue;
@property (nullable, readonly, copy) NSDate *dateValue;
@property (nullable, readonly, copy) NSURL *fileURLValue;
@property (readonly) AEEventClass eventClass;
@property (readonly) AEEventID eventID;
@property (readonly) AEReturnID returnID;
@property (readonly) AETransactionID transactionID;
@property (readonly) BOOL isRecordDescriptor;
@property (readonly) NSInteger numberOfItems;

- (void)setParamDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword;
- (nullable NSAppleEventDescriptor *)paramDescriptorForKeyword:(AEKeyword)keyword;
- (void)removeParamDescriptorWithKeyword:(AEKeyword)keyword;
- (void)setAttributeDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword;
- (nullable NSAppleEventDescriptor *)attributeDescriptorForKeyword:(AEKeyword)keyword;
- (nullable NSAppleEventDescriptor *)sendEventWithOptions:(NSAppleEventSendOptions)sendOptions timeout:(NSTimeInterval)timeoutInSeconds
                                                   error:(NSError **)error;
- (void)insertDescriptor:(NSAppleEventDescriptor *)descriptor atIndex:(NSInteger)index;
- (nullable NSAppleEventDescriptor *)descriptorAtIndex:(NSInteger)index;
- (void)removeDescriptorAtIndex:(NSInteger)index;
- (void)setDescriptor:(NSAppleEventDescriptor *)descriptor forKeyword:(AEKeyword)keyword;
- (nullable NSAppleEventDescriptor *)descriptorForKeyword:(AEKeyword)keyword;
- (void)removeDescriptorWithKeyword:(AEKeyword)keyword;
- (AEKeyword)keywordForDescriptorAtIndex:(NSInteger)index;
- (nullable NSAppleEventDescriptor *)coerceToDescriptorType:(DescType)descriptorType;

@end

NS_ASSUME_NONNULL_END
