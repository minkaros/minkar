/* Copyright (c) 2006-2007 Christopher J. W. Lloyd

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE. */

#import <Foundation/NSException.h>
#import <Foundation/NSDate.h>
#import <Foundation/NSNotification.h>

@class NSDictionary<KeyType, ObjectType>, NSMutableDictionary<KeyType, ObjectType>, NSAutoreleasePool, NSLock;

NS_ASSUME_NONNULL_BEGIN

FOUNDATION_EXPORT NSNotificationName const NSDidBecomeSingleThreadedNotification;
FOUNDATION_EXPORT NSNotificationName const NSWillBecomeMultiThreadedNotification;
FOUNDATION_EXPORT NSNotificationName const NSThreadWillExitNotification;

@interface NSThread : NSObject {
    NSMutableDictionary *_dictionary;
    NSMutableDictionary *_sharedObjects;
    NSLock *_sharedObjectLock;
    NSAutoreleasePool *_currentPool;
    NSExceptionFrame *_currentHandler;
    NSString *_name;
    SEL _selector;
    id _argument;
    id _target;
    BOOL _cancelled;
    BOOL _executing;
    BOOL _finished;
    double _priority;
    NSQualityOfService _qualityOfService;
}

@property (class, readonly, strong) NSThread *currentThread;
@property (class, readonly, strong) NSThread *mainThread;
@property (class, readonly) BOOL isMainThread;

+ (void)detachNewThreadWithBlock:(void (^)(void))block;
+ (void)detachNewThreadSelector:(SEL)selector toTarget:(id)target withObject:(nullable id)argument;

+ (BOOL)isMultiThreaded;

@property (readonly, retain) NSMutableDictionary *threadDictionary;

+ (void)sleepUntilDate:(NSDate *)date;
+ (void)sleepForTimeInterval:(NSTimeInterval)ti;

+ (void)exit;

+ (double)threadPriority;
+ (BOOL)setThreadPriority:(double)p;

@property double threadPriority;
@property NSQualityOfService qualityOfService;

@property (class, readonly, copy) NSArray<NSNumber *> *callStackReturnAddresses;
@property (class, readonly, copy) NSArray<NSString *> *callStackSymbols;

@property (nullable, copy) NSString *name;
@property NSUInteger stackSize;

@property (readonly) BOOL isMainThread;

- (instancetype)init NS_DESIGNATED_INITIALIZER;
- (instancetype)initWithTarget:(id)target selector:(SEL)selector object:(nullable id)argument;
- (instancetype)initWithBlock:(void (^)(void))block;

@property (readonly, getter=isExecuting) BOOL executing;
@property (readonly, getter=isFinished) BOOL finished;
@property (readonly, getter=isCancelled) BOOL cancelled;

- (void)cancel;
- (void)start;
- (void)main;

// private
- (NSMutableDictionary *)sharedDictionary;
- (void)setSharedObject:(id)object forClassName:(NSString *)className;

@end

@interface NSObject (NSThreadPerformAdditions)
- (void)performSelectorOnMainThread:(SEL)aSelector withObject:(nullable id)arg waitUntilDone:(BOOL)wait modes:(nullable NSArray<NSString *> *)array;
- (void)performSelectorOnMainThread:(SEL)aSelector withObject:(nullable id)arg waitUntilDone:(BOOL)wait;
- (void)performSelector:(SEL)aSelector onThread:(NSThread *)thr withObject:(nullable id)arg waitUntilDone:(BOOL)wait modes:(nullable NSArray<NSString *> *)array;
- (void)performSelector:(SEL)aSelector onThread:(NSThread *)thr withObject:(nullable id)arg waitUntilDone:(BOOL)wait;
- (void)performSelectorInBackground:(SEL)aSelector withObject:(nullable id)arg;
@end

NS_ASSUME_NONNULL_END

// PRIVATE
FOUNDATION_EXPORT NSThread *NSCurrentThread(void);
FOUNDATION_EXPORT id NSThreadSharedInstance(NSString *className);
FOUNDATION_EXPORT id NSThreadSharedInstanceDoNotCreate(NSString *className);
