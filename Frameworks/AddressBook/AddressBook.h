// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
//
// The address book. There is no contacts database yet: the book holds one
// person, the user ("me"), with the name from the user's account.

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const kABFirstNameProperty;
FOUNDATION_EXPORT NSString *const kABLastNameProperty;
FOUNDATION_EXPORT NSString *const kABNicknameProperty;
FOUNDATION_EXPORT NSString *const kABOrganizationProperty;
FOUNDATION_EXPORT NSString *const kABEmailProperty;
FOUNDATION_EXPORT NSString *const kABPhoneProperty;
FOUNDATION_EXPORT NSString *const kABAddressProperty;
FOUNDATION_EXPORT NSString *const kABUIDProperty;
FOUNDATION_EXPORT NSString *const kABEmailWorkLabel;
FOUNDATION_EXPORT NSString *const kABEmailHomeLabel;
FOUNDATION_EXPORT NSString *const kABOtherLabel;
FOUNDATION_EXPORT NSString *const kABDatabaseChangedExternallyNotification;

@interface ABMultiValue : NSObject <NSCopying, NSFastEnumeration>
- (NSUInteger)count;
- (id)valueAtIndex:(NSUInteger)index;
- (NSString *)labelAtIndex:(NSUInteger)index;
- (NSString *)identifierAtIndex:(NSUInteger)index;
- (NSUInteger)indexForIdentifier:(NSString *)identifier;
- (nullable NSString *)primaryIdentifier;
@end

@interface ABMutableMultiValue : ABMultiValue
- (NSString *)addValue:(id)value withLabel:(NSString *)label;
- (BOOL)setPrimaryIdentifier:(NSString *)identifier;
@end

@interface ABRecord : NSObject
- (nullable id)valueForProperty:(NSString *)property;
- (BOOL)setValue:(nullable id)value forProperty:(NSString *)property;
- (BOOL)removeValueForProperty:(NSString *)property;
@property (readonly, copy) NSString *uniqueId;
@end

@interface ABPerson : ABRecord
@end

@interface ABAddressBook : NSObject
+ (nullable ABAddressBook *)sharedAddressBook;
+ (nullable ABAddressBook *)addressBook;
- (nullable ABPerson *)me;
- (NSArray<ABPerson *> *)people;
- (nullable ABRecord *)recordForUniqueId:(NSString *)uniqueId;
- (BOOL)save;
- (BOOL)hasUnsavedChanges;
@end

NS_ASSUME_NONNULL_END
