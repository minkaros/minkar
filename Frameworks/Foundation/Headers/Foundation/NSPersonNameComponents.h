/* SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * The parts of a person's name (API 19).
 */

#import <Foundation/NSObject.h>

@class NSString;

NS_ASSUME_NONNULL_BEGIN

@interface NSPersonNameComponents : NSObject <NSCopying, NSSecureCoding>

@property (copy, nullable) NSString *namePrefix;
@property (copy, nullable) NSString *givenName;
@property (copy, nullable) NSString *middleName;
@property (copy, nullable) NSString *familyName;
@property (copy, nullable) NSString *nameSuffix;
@property (copy, nullable) NSString *nickname;
@property (copy, nullable) NSPersonNameComponents *phoneticRepresentation;

@end

NS_ASSUME_NONNULL_END
