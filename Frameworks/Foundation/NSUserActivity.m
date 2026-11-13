/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 */

#import <Foundation/NSUserActivity.h>
#import <Foundation/NSDictionary.h>
#import <Foundation/NSError.h>
#import <Foundation/NSSet.h>
#import <Foundation/NSString.h>
#import <Foundation/NSURL.h>
#import <Foundation/NSException.h>
#include <dispatch/dispatch.h>

NSString *const NSUserActivityTypeBrowsingWeb = @"NSUserActivityTypeBrowsingWeb";

/* The process's current activity: the last one made current and not resigned. */
static __weak NSUserActivity *currentActivity;

@implementation NSUserActivity {
    NSString *_activityType;
    BOOL _invalidated;
}

- (instancetype)initWithActivityType:(NSString *)activityType {
    if ((self = [super init]) != nil) {
        _activityType = [activityType copy];
        _keywords = [NSSet set];
        _eligibleForHandoff = YES;
    }
    return self;
}

- (instancetype)init {
    return [self initWithActivityType:@""];
}

- (NSString *)activityType {
    return _activityType;
}

- (void)setWebpageURL:(NSURL *)webpageURL {
    /* Only web addresses, as the platform requires. */
    NSString *scheme = [[webpageURL scheme] lowercaseString];
    if (webpageURL != nil && ![scheme isEqualToString:@"http"] && ![scheme isEqualToString:@"https"])
        [NSException raise:NSInvalidArgumentException format:@"%@ is not an http or https URL", webpageURL];
    _webpageURL = [webpageURL copy];
}

- (void)addUserInfoEntriesFromDictionary:(NSDictionary *)otherDictionary {
    NSMutableDictionary *info = [NSMutableDictionary dictionaryWithDictionary:_userInfo ?: @{}];
    [info addEntriesFromDictionary:otherDictionary];
    _userInfo = [info copy];
}

- (void)becomeCurrent {
    if (_invalidated)
        return;
    if (_needsSave && [_delegate respondsToSelector:@selector(userActivityWillSave:)]) {
        [_delegate userActivityWillSave:self];
        _needsSave = NO;
    }
    @synchronized ([NSUserActivity class]) {
        currentActivity = self;
    }
}

- (void)resignCurrent {
    @synchronized ([NSUserActivity class]) {
        if (currentActivity == self)
            currentActivity = nil;
    }
}

- (void)invalidate {
    [self resignCurrent];
    _invalidated = YES;
}

- (void)getContinuationStreamsWithCompletionHandler:(void (^)(NSInputStream *, NSOutputStream *, NSError *))completionHandler {
    /* No other device can continue it, so there are no streams. */
    NSError *error = [NSError errorWithDomain:NSCocoaErrorDomain code:4609 /* NSUserActivityConnectionUnavailableError */
                                     userInfo:nil];
    dispatch_async(dispatch_get_main_queue(), ^{
        completionHandler(nil, nil, error);
    });
}

/* Activities are not saved on Minkar: there is nothing to delete. */
+ (void)deleteSavedUserActivitiesWithPersistentIdentifiers:(NSArray *)persistentIdentifiers
                                         completionHandler:(void (^)(void))handler {
    dispatch_async(dispatch_get_main_queue(), handler);
}

+ (void)deleteAllSavedUserActivitiesWithCompletionHandler:(void (^)(void))handler {
    dispatch_async(dispatch_get_main_queue(), handler);
}

- (NSString *)description {
    return [NSString stringWithFormat:@"<%@ %p %@%@%@>", [self class], self, _activityType,
        _title ? [@" " stringByAppendingString:_title] : @"",
        _webpageURL ? [@" " stringByAppendingString:[_webpageURL absoluteString]] : @""];
}

@end
