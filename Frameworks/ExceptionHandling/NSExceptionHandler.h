/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * NSExceptionHandler: which exceptions a program logs and handles (those
 * nobody catches, and ones raised while being caught), as masks, and a
 * delegate asked about each.
 */

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSString *const NSUncaughtSystemExceptionException;
FOUNDATION_EXPORT NSString *const NSUncaughtRuntimeErrorException;
/* The key of the call stack (return addresses, as hexadecimal text) in an
   exception's userInfo once it is logged. */
FOUNDATION_EXPORT NSString *const NSStackTraceKey;

FOUNDATION_EXPORT void NSExceptionHandlerResume(void);

enum {
    NSLogUncaughtExceptionMask = 1 << 0,
    NSHandleUncaughtExceptionMask = 1 << 1,
    NSLogUncaughtSystemExceptionMask = 1 << 2,
    NSHandleUncaughtSystemExceptionMask = 1 << 3,
    NSLogUncaughtRuntimeErrorMask = 1 << 4,
    NSHandleUncaughtRuntimeErrorMask = 1 << 5,
    NSLogTopLevelExceptionMask = 1 << 6,
    NSHandleTopLevelExceptionMask = 1 << 7,
    NSLogOtherExceptionMask = 1 << 8,
    NSHandleOtherExceptionMask = 1 << 9,
};
#define NSLogAndHandleEveryExceptionMask (NSLogUncaughtExceptionMask | NSLogUncaughtSystemExceptionMask | \
    NSLogUncaughtRuntimeErrorMask | NSHandleUncaughtExceptionMask | NSHandleUncaughtSystemExceptionMask | \
    NSHandleUncaughtRuntimeErrorMask | NSLogTopLevelExceptionMask | NSHandleTopLevelExceptionMask | \
    NSLogOtherExceptionMask | NSHandleOtherExceptionMask)

enum {
    NSHangOnUncaughtExceptionMask = 1 << 0,
    NSHangOnUncaughtSystemExceptionMask = 1 << 1,
    NSHangOnUncaughtRuntimeErrorMask = 1 << 2,
    NSHangOnTopLevelExceptionMask = 1 << 3,
    NSHangOnOtherExceptionMask = 1 << 4,
};
#define NSHangOnEveryExceptionMask (NSHangOnUncaughtExceptionMask | NSHangOnUncaughtSystemExceptionMask | \
    NSHangOnUncaughtRuntimeErrorMask | NSHangOnTopLevelExceptionMask | NSHangOnOtherExceptionMask)

@interface NSExceptionHandler : NSObject

+ (NSExceptionHandler *)defaultExceptionHandler;

@property NSUInteger exceptionHandlingMask;
@property NSUInteger exceptionHangingMask;
@property (nullable, assign) id delegate;

@end

/* What the delegate may answer. */
@interface NSObject (NSExceptionHandlerDelegate)
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldLogException:(NSException *)exception mask:(NSUInteger)aMask;
- (BOOL)exceptionHandler:(NSExceptionHandler *)sender shouldHandleException:(NSException *)exception mask:(NSUInteger)aMask;
@end

NS_ASSUME_NONNULL_END
