// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.
// Built with ARC.

#import <AddressBook/AddressBook.h>

NSString *const kABFirstNameProperty = @"First";
NSString *const kABLastNameProperty = @"Last";
NSString *const kABNicknameProperty = @"Nickname";
NSString *const kABOrganizationProperty = @"Organization";
NSString *const kABEmailProperty = @"Email";
NSString *const kABPhoneProperty = @"Phone";
NSString *const kABAddressProperty = @"Address";
NSString *const kABUIDProperty = @"UID";
NSString *const kABEmailWorkLabel = @"_$!<Work>!$_";
NSString *const kABEmailHomeLabel = @"_$!<Home>!$_";
NSString *const kABOtherLabel = @"_$!<Other>!$_";
NSString *const kABDatabaseChangedExternallyNotification = @"ABDatabaseChangedExternallyNotification";
NSString *const kABGroupNameProperty = @"GroupName";
NSString *const kABModificationDateProperty = @"Modification";
NSString *const kABCreationDateProperty = @"Creation";
NSString *const kABAddressCityKey = @"City";
NSString *const kABAddressCountryKey = @"Country";
NSString *const kABAddressStateKey = @"State";
NSString *const kABAddressStreetKey = @"Street";
NSString *const kABAddressZIPKey = @"ZIP";
NSString *const kABAddressHomeLabel = @"_$!<Home>!$_";
NSString *const kABAddressWorkLabel = @"_$!<Work>!$_";
NSString *const kABAnniversaryLabel = @"_$!<Anniversary>!$_";
NSString *const kABBirthdayProperty = @"Birthday";
NSString *const kABDepartmentProperty = @"ABDepartment";
NSString *const kABHomePageProperty = @"HomePage";
NSString *const kABInstantMessageProperty = @"InstantMessage";
NSString *const kABJobTitleProperty = @"JobTitle";
NSString *const kABNoteProperty = @"Note";
NSString *const kABOtherDatesProperty = @"ABDate";
NSString *const kABPersonFlags = @"ABPersonFlags";
NSString *const kABPhoneHomeLabel = @"_$!<Home>!$_";
NSString *const kABPhoneMobileLabel = @"_$!<Mobile>!$_";
NSString *const kABPhonePagerLabel = @"_$!<Pager>!$_";
NSString *const kABPhoneWorkFAXLabel = @"_$!<WorkFAX>!$_";
NSString *const kABPhoneWorkLabel = @"_$!<Work>!$_";
NSString *const kABInsertedRecords = @"ABInsertedRecords";
NSString *const kABUpdatedRecords = @"ABUpdatedRecords";
NSString *const kABDeletedRecords = @"ABDeletedRecords";
NSString *const kABDatabaseChangedNotification = @"ABDatabaseChangedNotification";

@implementation ABMultiValue {
@protected
    NSMutableArray *_values, *_labels, *_identifiers;
    NSString *_primary;
}
- (instancetype)init {
    if ((self = [super init])) {
        _values = [NSMutableArray array];
        _labels = [NSMutableArray array];
        _identifiers = [NSMutableArray array];
    }
    return self;
}
- (id)copyWithZone:(NSZone *)zone {
    ABMultiValue *copy = [ABMultiValue new];
    [copy->_values setArray:_values];
    [copy->_labels setArray:_labels];
    [copy->_identifiers setArray:_identifiers];
    copy->_primary = _primary;
    return copy;
}
- (NSUInteger)count { return _values.count; }
- (id)valueAtIndex:(NSUInteger)index { return index < _values.count ? _values[index] : nil; }
- (NSString *)labelAtIndex:(NSUInteger)index { return index < _labels.count ? _labels[index] : nil; }
- (NSString *)identifierAtIndex:(NSUInteger)index { return index < _identifiers.count ? _identifiers[index] : nil; }
- (NSUInteger)indexForIdentifier:(NSString *)identifier { return [_identifiers indexOfObject:identifier]; }
- (NSString *)primaryIdentifier { return _primary ?: _identifiers.firstObject; }
- (NSUInteger)countByEnumeratingWithState:(NSFastEnumerationState *)state objects:(id __unsafe_unretained [])buffer count:(NSUInteger)len {
    NSArray *values = _values;
    return [values countByEnumeratingWithState:state objects:(__autoreleasing id *)(void *)buffer count:len];
}
@end

@implementation ABMutableMultiValue
- (NSString *)addValue:(id)value withLabel:(NSString *)label {
    NSString *identifier = [[NSUUID UUID] UUIDString];
    [_values addObject:value];
    [_labels addObject:label ?: kABOtherLabel];
    [_identifiers addObject:identifier];
    return identifier;
}
- (BOOL)setPrimaryIdentifier:(NSString *)identifier {
    if (![_identifiers containsObject:identifier])
        return NO;
    _primary = [identifier copy];
    return YES;
}
@end

@implementation ABRecord {
    NSMutableDictionary *_properties;
}
- (instancetype)init {
    if ((self = [super init])) {
        _properties = [NSMutableDictionary dictionary];
        _properties[kABUIDProperty] = [[NSUUID UUID] UUIDString];
    }
    return self;
}
- (NSString *)uniqueId { return _properties[kABUIDProperty]; }
- (id)valueForProperty:(NSString *)property { return _properties[property]; }
- (BOOL)setValue:(id)value forProperty:(NSString *)property {
    if (value == nil)
        [_properties removeObjectForKey:property];
    else
        _properties[property] = value;
    return YES;
}
- (BOOL)removeValueForProperty:(NSString *)property {
    [_properties removeObjectForKey:property];
    return YES;
}
@end

@implementation ABPerson
- (NSArray *)parentGroups { return @[]; }
@end

@implementation ABGroup
- (NSArray *)members { return @[]; }
- (NSArray *)subgroups { return @[]; }
- (NSArray *)parentGroups { return @[]; }
- (BOOL)addMember:(ABPerson *)person { return NO; }
- (BOOL)removeMember:(ABPerson *)person { return NO; }
@end

@implementation ABAddressBook {
    ABPerson *_me;
}

+ (ABAddressBook *)sharedAddressBook {
    static ABAddressBook *book;
    static dispatch_once_t once;
    dispatch_once(&once, ^{ book = [self new]; });
    return book;
}

+ (ABAddressBook *)addressBook {
    return [self new];
}

/* The user: first and last name from the account's full name. */
- (ABPerson *)me {
    @synchronized (self) {
        if (_me == nil) {
            NSString *full = NSFullUserName();
            NSArray *words = [full componentsSeparatedByCharactersInSet:[NSCharacterSet whitespaceCharacterSet]];
            _me = [ABPerson new];
            if (words.count > 0 && [words[0] length] > 0)
                [_me setValue:words[0] forProperty:kABFirstNameProperty];
            if (words.count > 1)
                [_me setValue:[[words subarrayWithRange:NSMakeRange(1, words.count - 1)] componentsJoinedByString:@" "]
                  forProperty:kABLastNameProperty];
        }
        return _me;
    }
}

- (NSArray *)people { return @[ [self me] ]; }
- (NSArray *)groups { return @[]; }
- (BOOL)addRecord:(ABRecord *)record { return NO; }
- (BOOL)removeRecord:(ABRecord *)record { return NO; }
- (ABRecord *)recordForUniqueId:(NSString *)uniqueId { return [[[self me] uniqueId] isEqual:uniqueId] ? [self me] : nil; }
- (BOOL)save { return YES; }
- (BOOL)hasUnsavedChanges { return NO; }
@end
