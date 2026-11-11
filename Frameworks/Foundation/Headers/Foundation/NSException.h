/* Copyright (c) 2006-2007 Christopher J. W. Lloyd
   Modifications Copyright (c) 2026 The Minkar contributors

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSObject.h>
#include <setjmp.h>

@class NSDictionary<KeyType, ObjectType>, NSArray<ObjectType>;

typedef NSString *NSExceptionName NS_TYPED_EXTENSIBLE_ENUM;

FOUNDATION_EXPORT NSString *const NSGenericException;
FOUNDATION_EXPORT NSString *const NSInvalidArgumentException;
FOUNDATION_EXPORT NSString *const NSPortTimeoutException;
FOUNDATION_EXPORT NSString *const NSInvalidSendPortException;
FOUNDATION_EXPORT NSString *const NSInvalidReceivePortException;
FOUNDATION_EXPORT NSString *const NSPortSendException;
FOUNDATION_EXPORT NSString *const NSPortReceiveException;
FOUNDATION_EXPORT NSString *const NSRangeException;

FOUNDATION_EXPORT NSString *const NSInternalInconsistencyException;
FOUNDATION_EXPORT NSString *const NSMallocException;

FOUNDATION_EXPORT NSString *const NSParseErrorException;
FOUNDATION_EXPORT NSString *const NSInconsistentArchiveException;

@interface NSException : NSObject <NSCoding, NSCopying> {
    NSString *_name;
    NSString *_reason;
    NSDictionary *_userInfo;
    NSArray *_callStack;
}

+ (void)raise:(NSString *)name format:(NSString *)format, ...;
+ (void)raise:(NSString *)name format:(NSString *)format arguments:(va_list)arguments;

- initWithName:(NSString *)name reason:(NSString *)reason userInfo:(NSDictionary *)userInfo;

+ (NSException *)exceptionWithName:(NSString *)name reason:(NSString *)reason userInfo:(NSDictionary *)userInfo;

- (void)raise;

- (NSString *)name;
- (NSString *)reason;
- (NSDictionary *)userInfo;

- (NSArray *)callStackReturnAddresses;

@end

typedef void NSUncaughtExceptionHandler(NSException *exception);

FOUNDATION_EXPORT NSUncaughtExceptionHandler *NSGetUncaughtExceptionHandler(void);
FOUNDATION_EXPORT void NSSetUncaughtExceptionHandler(NSUncaughtExceptionHandler *);

typedef struct NSExceptionFrame {
    jmp_buf state;
    struct NSExceptionFrame *parent;
    NSException *exception;
} NSExceptionFrame;

FOUNDATION_EXPORT void __NSPushExceptionFrame(NSExceptionFrame *frame);
FOUNDATION_EXPORT void __NSPopExceptionFrame(NSExceptionFrame *frame);

/* Minkar: the handler macros are Objective-C's exception statements, as on
   the reference platform, so they catch what objc_exception_throw raises
   (the setjmp frames above, kept for binaries built against them, never
   saw those exceptions). */
#define NS_DURING @try {

#define NS_HANDLER \
    }              \
    @catch(NSException * localException) {

#define NS_ENDHANDLER \
    localException = nil; /* unused */ \
    }

#define NS_VALUERETURN(val, type) return (val)

#define NS_VOIDRETURN return

#import <Foundation/NSAssertionHandler.h>
