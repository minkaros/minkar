// SPDX-License-Identifier: BSD-2-Clause
// Copyright (c) 2026 The Minkar contributors
// Part of Minkar.

#import <Foundation/NSObject.h>
#import <Foundation/NSComparisonPredicate.h>

@class NSString;

/* A comparison's operator as the reference archives it. */
@interface _NSArchivedPredicateOperator : NSObject <NSCoding>
@property (readonly) NSInteger operatorType;
@property (readonly) NSInteger modifier;
@property (readonly) NSUInteger options;
@property (readonly) BOOL negate;
@property (readonly) SEL customSelector;
- (NSPredicateOperatorType)predicateOperatorType;
@end

/* The class an archive class of the reference's predicates is read as
   (Nil for one not of them). */
Class _NSPredicateArchiveClassForName(NSString *name);
