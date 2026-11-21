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
// The values measured on the reference platform (2026-11-21).
FOUNDATION_EXPORT NSString *const kABGroupNameProperty;
FOUNDATION_EXPORT NSString *const kABModificationDateProperty;
FOUNDATION_EXPORT NSString *const kABCreationDateProperty;
FOUNDATION_EXPORT NSString *const kABAddressCityKey;
FOUNDATION_EXPORT NSString *const kABAddressCountryKey;
FOUNDATION_EXPORT NSString *const kABAddressStateKey;
FOUNDATION_EXPORT NSString *const kABAddressStreetKey;
FOUNDATION_EXPORT NSString *const kABAddressZIPKey;
FOUNDATION_EXPORT NSString *const kABAddressHomeLabel;
FOUNDATION_EXPORT NSString *const kABAddressWorkLabel;
FOUNDATION_EXPORT NSString *const kABAnniversaryLabel;
FOUNDATION_EXPORT NSString *const kABBirthdayProperty;
FOUNDATION_EXPORT NSString *const kABDepartmentProperty;
FOUNDATION_EXPORT NSString *const kABHomePageProperty;
FOUNDATION_EXPORT NSString *const kABInstantMessageProperty;
FOUNDATION_EXPORT NSString *const kABJobTitleProperty;
FOUNDATION_EXPORT NSString *const kABNoteProperty;
FOUNDATION_EXPORT NSString *const kABOtherDatesProperty;
FOUNDATION_EXPORT NSString *const kABPersonFlags;
FOUNDATION_EXPORT NSString *const kABPhoneHomeLabel;
FOUNDATION_EXPORT NSString *const kABPhoneMobileLabel;
FOUNDATION_EXPORT NSString *const kABPhonePagerLabel;
FOUNDATION_EXPORT NSString *const kABPhoneWorkFAXLabel;
FOUNDATION_EXPORT NSString *const kABPhoneWorkLabel;
FOUNDATION_EXPORT NSString *const kABInsertedRecords;
FOUNDATION_EXPORT NSString *const kABUpdatedRecords;
FOUNDATION_EXPORT NSString *const kABDeletedRecords;
FOUNDATION_EXPORT NSString *const kABDatabaseChangedNotification;

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
- (NSArray *)parentGroups;
@end

// A group of people. The book has no groups yet: they are all empty.
@interface ABGroup : ABRecord
- (NSArray<ABPerson *> *)members;
- (NSArray<ABGroup *> *)subgroups;
- (NSArray<ABGroup *> *)parentGroups;
- (BOOL)addMember:(ABPerson *)person;
- (BOOL)removeMember:(ABPerson *)person;
@end

@interface ABAddressBook : NSObject
+ (nullable ABAddressBook *)sharedAddressBook;
+ (nullable ABAddressBook *)addressBook;
- (nullable ABPerson *)me;
- (NSArray<ABPerson *> *)people;
- (NSArray<ABGroup *> *)groups;
- (BOOL)addRecord:(ABRecord *)record;
- (BOOL)removeRecord:(ABRecord *)record;
- (nullable ABRecord *)recordForUniqueId:(NSString *)uniqueId;
- (BOOL)save;
- (BOOL)hasUnsavedChanges;
@end

NS_ASSUME_NONNULL_END
