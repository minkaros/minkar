/*
 * SPDX-License-Identifier: BSD-2-Clause
 * Copyright (c) 2026 The Minkar contributors
 * Part of Minkar.
 *
 * Exceptions nobody catches reach the handler through Foundation's
 * uncaught exception handler: they are logged (with their call stack) and
 * handed to the delegate as the masks say. The handler before it is
 * still called, so the program ends as it would have. Every exception
 * raised (caught or not) passes objc4's exception preprocessor, where the
 * "other exception" masks apply.
 */

#import "NSExceptionHandler.h"
#import <objc/objc-exception.h>
#include <stdio.h>
#include <unistd.h>

NSString *const NSUncaughtSystemExceptionException = @"NSUncaughtSystemExceptionException";
NSString *const NSUncaughtRuntimeErrorException = @"NSUncaughtRuntimeErrorException";
NSString *const NSStackTraceKey = @"NSStackTraceKey";

static NSUncaughtExceptionHandler *previousHandler;
static objc_exception_preprocessor previousPreprocessor;
static volatile int resumed;

void NSExceptionHandlerResume(void) {
    resumed = 1;
}

static NSString *stackTrace(NSException *exception) {
    NSMutableArray *addresses = [NSMutableArray array];
    for (NSNumber *a in [exception callStackReturnAddresses])
        [addresses addObject:[NSString stringWithFormat:@"0x%lx", (unsigned long)[a unsignedLongValue]]];
    return [addresses componentsJoinedByString:@"  "];
}

@implementation NSExceptionHandler {
    id _delegate;
}

@synthesize exceptionHandlingMask = _exceptionHandlingMask;
@synthesize exceptionHangingMask = _exceptionHangingMask;

static void uncaught(NSException *exception) {
    NSExceptionHandler *handler = [NSExceptionHandler defaultExceptionHandler];
    NSUInteger mask = handler.exceptionHandlingMask;
    id delegate = handler.delegate;

    if (mask & NSLogUncaughtExceptionMask) {
        BOOL log = YES;
        if ([delegate respondsToSelector:@selector(exceptionHandler:shouldLogException:mask:)])
            log = [delegate exceptionHandler:handler shouldLogException:exception mask:NSLogUncaughtExceptionMask];
        if (log)
            NSLog(@"*** Uncaught exception %@: %@\nStack trace: %@", [exception name], [exception reason],
                  stackTrace(exception));
    }
    if ((mask & NSHandleUncaughtExceptionMask) &&
        [delegate respondsToSelector:@selector(exceptionHandler:shouldHandleException:mask:)])
        [delegate exceptionHandler:handler shouldHandleException:exception mask:NSHandleUncaughtExceptionMask];
    if (handler.exceptionHangingMask & NSHangOnUncaughtExceptionMask) {
        fprintf(stderr, "*** process %d hangs on an uncaught exception (NSExceptionHandlerResume to go on)\n", getpid());
        while (!resumed)
            sleep(1);
    }
    if (previousHandler)
        previousHandler(exception);
}

static id raised(id object) {
    NSExceptionHandler *handler = [NSExceptionHandler defaultExceptionHandler];
    NSUInteger mask = handler.exceptionHandlingMask;
    id delegate = handler.delegate;

    if ([object isKindOfClass:[NSException class]] && (mask & (NSLogOtherExceptionMask | NSHandleOtherExceptionMask))) {
        NSException *exception = object;
        if (mask & NSLogOtherExceptionMask) {
            BOOL log = YES;
            if ([delegate respondsToSelector:@selector(exceptionHandler:shouldLogException:mask:)])
                log = [delegate exceptionHandler:handler shouldLogException:exception mask:NSLogOtherExceptionMask];
            if (log)
                NSLog(@"*** Exception %@: %@", [exception name], [exception reason]);
        }
        if ((mask & NSHandleOtherExceptionMask) &&
            [delegate respondsToSelector:@selector(exceptionHandler:shouldHandleException:mask:)])
            [delegate exceptionHandler:handler shouldHandleException:exception mask:NSHandleOtherExceptionMask];
    }
    return previousPreprocessor ? previousPreprocessor(object) : object;
}

+ (NSExceptionHandler *)defaultExceptionHandler {
    static NSExceptionHandler *handler;
    static dispatch_once_t once;
    dispatch_once(&once, ^{
        handler = [NSExceptionHandler new];
        previousHandler = NSGetUncaughtExceptionHandler();
        NSSetUncaughtExceptionHandler(uncaught);
        previousPreprocessor = objc_setExceptionPreprocessor(raised);
    });
    return handler;
}

- (id)delegate {
    return _delegate;
}

- (void)setDelegate:(id)delegate {
    _delegate = delegate;
}

@end
