/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import "MenuExtra.h"

NS_ASSUME_NONNULL_BEGIN

/* The clock: day, date and time in the menu bar, the full date in its
   menu. It wakes once a minute, on the minute. */
@interface ClockExtra : NSObject <MenuExtra>
/* The menu bar's text for a date (exposed for the tests). */
+ (NSString *)titleForDate:(NSDate *)date;
@end

NS_ASSUME_NONNULL_END
